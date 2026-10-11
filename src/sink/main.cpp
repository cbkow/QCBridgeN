// SPDX-License-Identifier: MIT
// QCBridgeN — the application: a menu-bar / tray item over BusSink.
//
//   QCBridgeN [--id ndi] [--name "QCView"] [--raster 1920x1080]
//             [--content composite|a|b] [--no-guides] [--no-annotations]
//             [--headless] [--seconds N]
//
// Settings persist under QSettings ("QCBridgeN"); the command line
// overrides them for one run. --headless runs without a tray (tests);
// --seconds quits after N seconds (tests).

#include "bus_sink.h"
#include "ndi_lib.h"

#include <QAction>
#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QTimer>

#include <cstdio>

namespace {

QIcon makeIcon(bool live)
{
    QPixmap pm(44, 44);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(live ? "#4cb050" : "#888888"), 4));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(6, 10, 32, 24, 4, 4);
    p.setBrush(QColor(live ? "#4cb050" : "#888888"));
    p.drawEllipse(QPoint(22, 22), 5, 5);
    return QIcon(pm);
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("QCBridgeN"));
    QCoreApplication::setApplicationName(QStringLiteral("QCBridgeN"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QCBRIDGEN_VERSION));
    app.setQuitOnLastWindowClosed(false);

    QCommandLineParser cli;
    cli.addHelpOption();
    cli.addVersionOption();
    cli.addOption({ "id", "Output id in QCView (the manifest's id).", "id" });
    cli.addOption({ "name", "NDI source name.", "name" });
    cli.addOption({ "raster", "Requested raster, WxH.", "WxH" });
    cli.addOption({ "content", "composite, a or b.", "what" });
    cli.addOption({ "no-guides", "Do not ask for safety guides." });
    cli.addOption({ "no-annotations", "Do not ask for note strokes." });
    cli.addOption({ "headless", "No tray item." });
    cli.addOption({ "seconds", "Quit after N seconds.", "N" });
    cli.process(app);

    QSettings settings;
    SinkConfig cfg;
    cfg.id         = (cli.isSet("id") ? cli.value("id") : settings.value("id", "ndi").toString()).toStdString();
    cfg.sourceName = (cli.isSet("name") ? cli.value("name") : settings.value("sourceName", "QCView").toString()).toStdString();
    const QString raster = cli.isSet("raster") ? cli.value("raster") : settings.value("raster", "1920x1080").toString();
    {
        const QStringList wh = raster.split('x');
        if (wh.size() == 2) { cfg.width = wh[0].toUInt(); cfg.height = wh[1].toUInt(); }
    }
    const QString content = cli.isSet("content") ? cli.value("content") : settings.value("content", "composite").toString();
    cfg.content = content == "a" ? qcbae::OutputContent::SideA
                : content == "b" ? qcbae::OutputContent::SideB : qcbae::OutputContent::Composite;
    if (cli.isSet("no-guides") || !settings.value("guides", true).toBool()) cfg.layers &= ~qcbae::kOutputLayerGuides;
    if (cli.isSet("no-annotations") || !settings.value("annotations", true).toBool()) cfg.layers &= ~qcbae::kOutputLayerAnnotations;
    // What the command line set becomes the default next time.
    settings.setValue("id", QString::fromStdString(cfg.id));
    settings.setValue("sourceName", QString::fromStdString(cfg.sourceName));
    settings.setValue("raster", raster);
    settings.setValue("content", content);

    NdiLib ndi;
    std::string err;
    if (!ndi.load(&err)) {
        std::fprintf(stderr, "QCBridgeN: %s\n", err.c_str());
        return 1;
    }
    std::printf("QCBridgeN %s: NDI runtime %s\n", QCBRIDGEN_VERSION, ndi.version().c_str());

    BusSink sink(ndi, cfg);
    sink.setQuitRequest([] { QMetaObject::invokeMethod(qApp, &QCoreApplication::quit, Qt::QueuedConnection); });

    QSystemTrayIcon *tray = nullptr;
    QAction *stateAct = nullptr, *detailAct = nullptr;
    if (!cli.isSet("headless") && QSystemTrayIcon::isSystemTrayAvailable()) {
        tray = new QSystemTrayIcon(makeIcon(false), &app);
        auto *menu = new QMenu();
        stateAct  = menu->addAction(QStringLiteral("QCBridgeN: starting"));
        stateAct->setEnabled(false);
        detailAct = menu->addAction(QStringLiteral("NDI '%1' · %2 · id %3")
                                        .arg(QString::fromStdString(cfg.sourceName), raster, QString::fromStdString(cfg.id)));
        detailAct->setEnabled(false);
        menu->addSeparator();
        QAction *quit = menu->addAction(QStringLiteral("Quit QCBridgeN"));
        QObject::connect(quit, &QAction::triggered, &app, &QCoreApplication::quit);
        tray->setContextMenu(menu);
        tray->setToolTip(QStringLiteral("QCBridgeN"));
        tray->show();
    }

    sink.start();

    QTimer refresh;
    refresh.setInterval(1000);
    bool wasLive = false;
    QObject::connect(&refresh, &QTimer::timeout, [&] {
        const SinkStats s = sink.stats();
        const bool live = s.producerAlive && s.frames > 0;
        if (tray) {
            if (live != wasLive) { tray->setIcon(makeIcon(live)); wasLive = live; }
            stateAct->setText(QStringLiteral("%1 — %2 fps, %3 receiver%4")
                                  .arg(QString::fromStdString(s.state)).arg(s.fps, 0, 'f', 0)
                                  .arg(s.receivers).arg(s.receivers == 1 ? "" : "s"));
            tray->setToolTip(QStringLiteral("QCBridgeN: %1").arg(QString::fromStdString(s.state)));
        }
    });
    refresh.start();

    if (cli.isSet("seconds")) {
        QTimer::singleShot(static_cast<int>(cli.value("seconds").toDouble() * 1000), &app, &QCoreApplication::quit);
    }

    const int rc = app.exec();
    sink.stop();
    const SinkStats s = sink.stats();
    std::printf("QCBridgeN: done — %llu frames, %llu audio packets (%llu gaps, %llu dropped), last %d receiver%s\n",
                static_cast<unsigned long long>(s.frames), static_cast<unsigned long long>(s.audioPackets),
                static_cast<unsigned long long>(s.audioGaps), static_cast<unsigned long long>(s.audioDropped),
                s.receivers, s.receivers == 1 ? "" : "s");
    return rc;
}
