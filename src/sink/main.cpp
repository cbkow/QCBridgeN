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
#include "settings_dialog.h"

#include <QAction>
#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QMenu>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QTimer>

#include <cstdio>
#include <memory>

namespace {

// The menu-bar mark: on macOS a template image (black with alpha) that the
// system tints for light and dark menu bars, live = full, idle = faded;
// elsewhere the coloured mark.
QIcon makeIcon(bool live)
{
#if defined(Q_OS_MACOS)
    QIcon icon(live ? QStringLiteral(":/icons/tray-mac-live.png") : QStringLiteral(":/icons/tray-mac-idle.png"));
    icon.setIsMask(true);
    return icon;
#else
    Q_UNUSED(live);
    return QIcon(QStringLiteral(":/icons/tray-win-64.png"));
#endif
}

void saveConfig(const SinkConfig &cfg)
{
    QSettings settings;
    settings.setValue("id", QString::fromStdString(cfg.id));
    settings.setValue("sourceName", QString::fromStdString(cfg.sourceName));
    settings.setValue("raster", QStringLiteral("%1x%2").arg(cfg.width).arg(cfg.height));
    settings.setValue("content", cfg.content == qcbae::OutputContent::SideA ? "a"
                               : cfg.content == qcbae::OutputContent::SideB ? "b" : "composite");
    settings.setValue("guides", (cfg.layers & qcbae::kOutputLayerGuides) != 0u);
    settings.setValue("annotations", (cfg.layers & qcbae::kOutputLayerAnnotations) != 0u);
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
    saveConfig(cfg);   // what the command line set becomes the default next time

    NdiLib ndi;
    std::string err;
    if (!ndi.load(&err)) {
        std::fprintf(stderr, "QCBridgeN: %s\n", err.c_str());
        return 1;
    }
    std::printf("QCBridgeN %s: NDI runtime %s\n", QCBRIDGEN_VERSION, ndi.version().c_str());

    auto quitRequest = [] { QMetaObject::invokeMethod(qApp, &QCoreApplication::quit, Qt::QueuedConnection); };
    auto sinkPtr = std::make_unique<BusSink>(ndi, cfg);
    sinkPtr->setQuitRequest(quitRequest);
    BusSink *sinkRaw = sinkPtr.get();
    auto sinkRef = [&sinkRaw]() -> BusSink & { return *sinkRaw; };

    QSystemTrayIcon *tray = nullptr;
    QAction *stateAct = nullptr, *detailAct = nullptr;
    if (!cli.isSet("headless") && QSystemTrayIcon::isSystemTrayAvailable()) {
        if (makeIcon(false).isNull()) std::fprintf(stderr, "QCBridgeN: tray icon resource missing — the menu-bar item would be blank\n");
        tray = new QSystemTrayIcon(makeIcon(false), &app);
        auto *menu = new QMenu();
        stateAct  = menu->addAction(QStringLiteral("QCBridgeN: starting"));
        stateAct->setEnabled(false);
        detailAct = menu->addAction(QStringLiteral("NDI '%1' · %2 · id %3")
                                        .arg(QString::fromStdString(cfg.sourceName), raster, QString::fromStdString(cfg.id)));
        detailAct->setEnabled(false);
        menu->addSeparator();
        QAction *settingsAct = menu->addAction(QStringLiteral("Settings…"));
        QObject::connect(settingsAct, &QAction::triggered, [&] {
            SettingsDialog dlg(cfg);
            if (dlg.exec() != QDialog::Accepted) return;
            cfg = dlg.config();
            saveConfig(cfg);
            // Restart the attach so the new request goes out; the NDI
            // source is re-created under the new name.
            sinkPtr->stop();
            sinkPtr = std::make_unique<BusSink>(ndi, cfg);
            sinkPtr->setQuitRequest(quitRequest);
            sinkRaw = sinkPtr.get();
            sinkPtr->start();
            detailAct->setText(QStringLiteral("NDI '%1' · %2x%3 · id %4")
                                   .arg(QString::fromStdString(cfg.sourceName)).arg(cfg.width).arg(cfg.height)
                                   .arg(QString::fromStdString(cfg.id)));
        });
        menu->addSeparator();
        QAction *quit = menu->addAction(QStringLiteral("Quit QCBridgeN"));
        QObject::connect(quit, &QAction::triggered, &app, &QCoreApplication::quit);
        tray->setContextMenu(menu);
        tray->setToolTip(QStringLiteral("QCBridgeN"));
        tray->show();
    }

    sinkPtr->start();

    QTimer refresh;
    refresh.setInterval(1000);
    bool wasLive = false;
    QObject::connect(&refresh, &QTimer::timeout, [&] {
        const SinkStats s = sinkRef().stats();
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
    sinkRef().stop();
    const SinkStats s = sinkRef().stats();
    std::printf("QCBridgeN: done — %llu frames, %llu audio packets (%llu gaps, %llu dropped), last %d receiver%s\n",
                static_cast<unsigned long long>(s.frames), static_cast<unsigned long long>(s.audioPackets),
                static_cast<unsigned long long>(s.audioGaps), static_cast<unsigned long long>(s.audioDropped),
                s.receivers, s.receivers == 1 ? "" : "s");
    return rc;
}
