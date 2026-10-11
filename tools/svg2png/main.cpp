// SPDX-License-Identifier: MIT
// svg2png — render an SVG to a PNG with alpha at a given size, optionally
// recolouring every fill and applying an opacity, for the app and tray
// icons. Deterministic on both platforms (Qt SVG), unlike the OS thumbnail
// tools, which flatten onto white.
//
//   svg2png in.svg out.png SIZE [--fill #000000] [--opacity 0.45]

#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QRegularExpression>
#include <QSvgRenderer>

#include <cstdio>
#include <cstdlib>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (argc < 4) { std::fprintf(stderr, "usage: svg2png in.svg out.png SIZE [--fill #rrggbb] [--opacity F]\n"); return 2; }
    const QString in = argv[1], out = argv[2];
    const int size = std::atoi(argv[3]);
    QString fill; double opacity = 1.0;
    for (int i = 4; i + 1 < argc; i += 2) {
        const QString k = argv[i];
        if (k == "--fill") fill = argv[i + 1];
        else if (k == "--opacity") opacity = std::atof(argv[i + 1]);
    }
    QFile f(in);
    if (!f.open(QIODevice::ReadOnly)) { std::fprintf(stderr, "cannot read %s\n", qPrintable(in)); return 1; }
    QByteArray svg = f.readAll();
    if (!fill.isEmpty()) {
        // The marks use one fill class; replace every fill colour.
        static const QRegularExpression re("fill:\\s*#[0-9a-fA-F]{6}");
        svg = QString::fromUtf8(svg).replace(re, "fill: " + fill).toUtf8();
    }
    QSvgRenderer r(svg);
    if (!r.isValid()) { std::fprintf(stderr, "invalid svg %s\n", qPrintable(in)); return 1; }
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(opacity);
    r.render(&p, QRectF(0, 0, size, size));
    p.end();
    if (!img.save(out, "PNG")) { std::fprintf(stderr, "cannot write %s\n", qPrintable(out)); return 1; }
    std::printf("%s -> %s %dx%d%s%s\n", qPrintable(in), qPrintable(out), size, size,
                fill.isEmpty() ? "" : " recoloured", opacity < 1.0 ? " translucent" : "");
    return 0;
}
