#include <QGuiApplication>
#include <QSvgRenderer>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QXmlStreamReader>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QSaveFile>
#include <QDir>
#include <QBuffer>
#include <QFileInfo>
#include <QSet>
#include <QMap>
#include <functional>
#include <QWindow>
#include <QExposeEvent>
#include <QBackingStore>
#include <QScreen>
#include <QTimer>
#include <QSysInfo>
#include <QRegularExpression>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
QString repository;
void require(bool condition, const QString &message)
{
    if (!condition)
        throw std::runtime_error(message.toStdString());
}
QByteArray read(const QString &path)
{
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Cannot read " + path);
    return file.readAll();
}
void write(const QString &path, const QByteArray &bytes)
{
    require(QDir().mkpath(QFileInfo(path).absolutePath()), "Cannot create output directory");
    QSaveFile file(path);
    require(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit(),
            "Cannot save " + path);
}
void save(const QImage &image, const QString &path)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    require(buffer.open(QIODevice::WriteOnly) && image.save(&buffer, "PNG"), "PNG encoder failed");
    write(path, bytes);
}
QImage render(const QString &path, QSize size)
{
    QSvgRenderer renderer(path);
    require(renderer.isValid(), "QtSvg rejected " + path);
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(size)));
    painter.end();
    return image;
}
bool samePixels(const QImage &a, const QImage &b)
{
    return a.convertToFormat(QImage::Format_ARGB32) == b.convertToFormat(QImage::Format_ARGB32);
}
QRect alphaBounds(const QImage &image)
{
    int left = image.width(), right = -1, top = image.height(), bottom = -1;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (qAlpha(image.pixel(x, y)) > 8) {
                left = std::min(left, x);
                right = std::max(right, x);
                top = std::min(top, y);
                bottom = std::max(bottom, y);
            }
    return right >= left ? QRect(QPoint(left, top), QPoint(right, bottom)) : QRect();
}
void validateSvg(const QString &path)
{
    const QSet<QString> allowed{
        "svg",     "title",    "desc",     "defs",   "linearGradient", "radialGradient",
        "stop",    "clipPath", "g",        "path",   "rect",           "circle",
        "ellipse", "line",     "polyline", "polygon"
    };
    QXmlStreamReader xml(read(path));
    QSet<QString> ids;
    QStringList references;
    bool viewBox = false;
    while (!xml.atEnd()) {
        xml.readNext();
        require(!xml.isDTD(), "SVG DTD is not supported");
        if (!xml.isStartElement())
            continue;
        require(allowed.contains(xml.name().toString()),
                "Unsupported SVG element: " + xml.name().toString());
        for (const auto &a : xml.attributes()) {
            const auto name = a.name().toString();
            const auto value = a.value().toString();
            require(!name.startsWith("on") && name != "href" && name != "filter"
                            && !value.contains("http:") && !value.contains("https:")
                            && !value.contains("data:"),
                    "External or executable SVG attribute");
            const QRegularExpression localReference("url\\(#([^)]*)\\)");
            auto urls = localReference.globalMatch(value);
            while (urls.hasNext())
                references.append(urls.next().captured(1));
            if (name == "id") {
                require(!ids.contains(value), "Duplicate SVG id: " + value);
                ids.insert(value);
            }
            if (xml.name() == u"svg" && name == "viewBox")
                viewBox = true;
        }
    }
    for (const auto &reference : references)
        require(ids.contains(reference), "Missing local SVG reference: " + reference);
    require(!xml.hasError() && viewBox, "Invalid XML / missing viewBox: " + path);
    QSvgRenderer renderer(path);
    require(renderer.isValid() && !renderer.animated(), "Invalid/animated SVG");
}
void be32(QByteArray &bytes, quint32 value)
{
    const auto encoded = qToBigEndian(value);
    bytes.append(reinterpret_cast<const char *>(&encoded), 4);
}
void le16(QByteArray &bytes, quint16 value)
{
    const auto encoded = qToLittleEndian(value);
    bytes.append(reinterpret_cast<const char *>(&encoded), 2);
}
void le32(QByteArray &bytes, quint32 value)
{
    const auto encoded = qToLittleEndian(value);
    bytes.append(reinterpret_cast<const char *>(&encoded), 4);
}
QByteArray png(const QImage &image)
{
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    require(image.save(&buffer, "PNG"), "PNG encoding failed");
    return data;
}
QString sourceForSize(int size)
{
    return repository + "/resources/branding/" + (size <= 32 ? "icon-small.svg" : "icon.svg");
}
void exportContainers(const QString &directory)
{
    const QList<int> sizes{ 16, 24, 32, 48, 64, 128, 256 };
    QByteArray ico;
    le16(ico, 0);
    le16(ico, 1);
    le16(ico, quint16(sizes.size()));
    QList<QByteArray> blocks;
    quint32 offset = 6 + quint32(sizes.size()) * 16;
    for (int size : sizes) {
        const auto block = png(render(sourceForSize(size), { size, size }));
        ico.append(char(size == 256 ? 0 : size));
        ico.append(char(size == 256 ? 0 : size));
        ico.append(char(0));
        ico.append(char(0));
        le16(ico, 1);
        le16(ico, 32);
        le32(ico, quint32(block.size()));
        le32(ico, offset);
        offset += quint32(block.size());
        blocks.append(block);
    }
    for (const auto &block : blocks)
        ico.append(block);
    write(directory + "/yalahoo.ico", ico);
    QByteArray chunks;
    const QList<int> macSizes{ 128, 256, 512, 1024 };
    const QList<QByteArray> types{ "ic07", "ic08", "ic09", "ic10" };
    for (int i = 0; i < macSizes.size(); ++i) {
        const auto block = png(render(sourceForSize(macSizes[i]), { macSizes[i], macSizes[i] }));
        chunks.append(types[i]);
        be32(chunks, quint32(block.size()) + 8);
        chunks.append(block);
    }
    QByteArray icns("icns");
    be32(icns, quint32(chunks.size()) + 8);
    icns.append(chunks);
    write(directory + "/yalahoo.icns", icns);
}
QJsonArray verifyContainers(const QString &directory)
{
    QJsonArray result;
    const auto ico = read(directory + "/yalahoo.ico");
    const auto bytes = reinterpret_cast<const uchar *>(ico.constData());
    require(ico.size() >= 118 && qFromLittleEndian<quint16>(bytes) == 0
                    && qFromLittleEndian<quint16>(bytes + 2) == 1
                    && qFromLittleEndian<quint16>(bytes + 4) == 7,
            "ICO directory header invalid");
    quint32 next = 118;
    const QList<int> sizes{ 16, 24, 32, 48, 64, 128, 256 };
    for (int i = 0; i < 7; ++i) {
        const auto entry = bytes + 6 + i * 16;
        int size = entry[0] == 0 ? 256 : entry[0];
        const auto length = qFromLittleEndian<quint32>(entry + 8);
        const auto offset = qFromLittleEndian<quint32>(entry + 12);
        require(size == sizes[i] && entry[1] == entry[0] && entry[2] == 0 && entry[3] == 0
                        && qFromLittleEndian<quint16>(entry + 4) == 1
                        && qFromLittleEndian<quint16>(entry + 6) == 32 && offset == next
                        && quint64(offset) + length <= quint64(ico.size()),
                "ICO entry invalid");
        const auto image = QImage::fromData(ico.mid(offset, length), "PNG");
        require(image.size() == QSize(size, size) && image.hasAlphaChannel(), "ICO PNG invalid");
        require(samePixels(image, render(sourceForSize(size), { size, size })),
                "ICO differs from current SVG");
        next += length;
        result.append(QJsonObject{ { "container", "ico" },
                                   { "size", size },
                                   { "offset", int(offset) },
                                   { "bytes", int(length) } });
    }
    require(next == ico.size(), "ICO trailing bytes");
    const auto icns = read(directory + "/yalahoo.icns");
    require(icns.size() >= 8 && icns.first(4) == "icns"
                    && qFromBigEndian<quint32>(
                               reinterpret_cast<const uchar *>(icns.constData() + 4))
                            == icns.size(),
            "ICNS header invalid");
    qsizetype cursor = 8;
    const QList<QByteArray> types{ "ic07", "ic08", "ic09", "ic10" };
    const QList<int> macSizes{ 128, 256, 512, 1024 };
    for (int i = 0; i < 4; ++i) {
        require(cursor + 8 <= icns.size(), "ICNS truncated header");
        const auto type = icns.mid(cursor, 4);
        const auto length = qFromBigEndian<quint32>(
                reinterpret_cast<const uchar *>(icns.constData() + cursor + 4));
        require(type == types[i] && length >= 8 && cursor + length <= icns.size(),
                "ICNS block invalid");
        const auto image = QImage::fromData(icns.mid(cursor + 8, length - 8), "PNG");
        require(image.size() == QSize(macSizes[i], macSizes[i]) && image.hasAlphaChannel()
                        && samePixels(
                                image,
                                render(sourceForSize(macSizes[i]), { macSizes[i], macSizes[i] })),
                "ICNS PNG differs from current SVG");
        result.append(QJsonObject{ { "container", "icns" },
                                   { "type", QString::fromLatin1(type) },
                                   { "size", macSizes[i] },
                                   { "bytes", int(length) } });
        cursor += length;
    }
    require(cursor == icns.size(), "ICNS unexpected chunks/trailing bytes");
    return result;
}
// Only the actual authoring grammar is accepted: absolute M/L/Q/C/V/Z.
// No Qt private SVG parser or external geometry dependency.
QPainterPath parseCenterline(const QString &d)
{
    const QRegularExpression token("[MLQCVZ]|[-+]?(?:[0-9]*\\.)?[0-9]+");
    QStringList tokens;
    auto matches = token.globalMatch(d);
    while (matches.hasNext())
        tokens.append(matches.next().captured());
    QString residue = d;
    residue.remove(token);
    residue.remove(QRegularExpression("[ ,\\t\\r\\n]"));
    require(residue.isEmpty(), "Unsupported reveal path grammar");
    int at = 0;
    QPainterPath p;
    auto number = [&]() {
        require(at < tokens.size(), "Truncated reveal coordinate");
        bool ok = false;
        double n = tokens[at++].toDouble(&ok);
        require(ok, "Invalid reveal coordinate");
        return n;
    };
    auto point = [&]() {
        const double x = number();
        const double y = number();
        return QPointF(x, y);
    };
    while (at < tokens.size()) {
        const QString command = tokens[at++];
        if (command == "M")
            p.moveTo(point());
        else if (command == "L")
            p.lineTo(point());
        else if (command == "V") {
            const double y = number();
            p.lineTo(p.currentPosition().x(), y);
        } else if (command == "Q") {
            const auto a = point();
            const auto b = point();
            p.quadTo(a, b);
        } else if (command == "C") {
            const auto a = point();
            const auto b = point();
            const auto c = point();
            p.cubicTo(a, b, c);
        } else if (command == "Z")
            p.closeSubpath();
        else
            require(false, "Invalid reveal command");
    }
    return p;
}
QJsonObject revealMapping()
{
    QXmlStreamReader xml(read(repository + "/resources/branding/logo-reveal.svg"));
    QJsonArray paths;
    QString group;
    double total = 0;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement())
            continue;
        if (xml.name() == u"g")
            group = xml.attributes().value("id").toString();
        if (xml.name() != u"path")
            continue;
        const auto id = xml.attributes().value("id").toString();
        const auto d = xml.attributes().value("d").toString();
        const auto path = parseCenterline(d);
        const double length = path.length();
        require(!id.isEmpty() && length > 0, "Empty reveal segment");
        paths.append(QJsonObject{ { "id", id },
                                  { "group", group },
                                  { "d", d },
                                  { "length", length },
                                  { "offset", total },
                                  { "strokeWidth", group == "rings" ? 18 : 16 } });
        total += length;
    }
    require(!xml.hasError() && paths.size() == 17, "Reveal segment inventory invalid");
    for (int i = 0; i < paths.size(); ++i) {
        auto entry = paths[i].toObject();
        entry["start"] = entry["offset"].toDouble() / total;
        entry["end"] = (entry["offset"].toDouble() + entry["length"].toDouble()) / total;
        paths[i] = entry;
    }
    return { { "schemaVersion", 1 },
             { "viewBox", QJsonArray{ 0, 0, 816, 208 } },
             { "totalLength", total },
             { "paths", paths },
             { "progressRule",
               "clamp((contourProgress-start)/(end-start),0,1); arc length, not Bezier parameter" },
             { "clockRule",
               "musicOpeningFrame.contour: settle(unit(t,0.24,1.64)); accents also use original "
               "symbols envelope; no duration change" },
             { "terminalRule",
               "reduced motion: show complete production logo; reveal at 1 replaces only inner "
               "groove details with full logo" } };
}
void paintReveal(QPainter &p, const QJsonObject &mapping, double progress, const QRectF &rect)
{
    p.save();
    p.translate(rect.topLeft());
    p.scale(rect.width() / 816, rect.height() / 208);
    for (const auto &value : mapping["paths"].toArray()) {
        const auto entry = value.toObject();
        const double t = std::clamp((progress - entry["start"].toDouble())
                                            / (entry["end"].toDouble() - entry["start"].toDouble()),
                                    0.0, 1.0);
        if (t <= 0)
            continue;
        auto path = parseCenterline(entry["d"].toString());
        const double length = path.length() * t;
        QPainterPath partial;
        partial.moveTo(path.pointAtPercent(0));
        const int steps = std::max(1, int(std::ceil(length / 0.5)));
        for (int step = 1; step <= steps; ++step)
            partial.lineTo(path.pointAtPercent(path.percentAtLength(length * step / steps)));
        QPen pen(QColor(entry["group"] == "accents" ? "#b56834" : "#232722"),
                 entry["strokeWidth"].toDouble(), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.drawPath(partial);
    }
    p.restore();
}
class PreviewWindow : public QWindow
{
    QBackingStore backing{ this };
    QImage board;

public:
    explicit PreviewWindow(QImage image) : board(std::move(image))
    {
        setTitle("yalahOO!!! · R03B Qt brand proof");
        resize(board.size());
    }
    void exposeEvent(QExposeEvent *) override
    {
        if (!isExposed())
            return;
        backing.resize(size());
        backing.beginPaint(QRect(QPoint(), size()));
        QPainter p(backing.paintDevice());
        p.drawImage(QRect(QPoint(), size()), board);
        p.end();
        backing.endPaint();
        backing.flush(QRect(QPoint(), size()));
    }
};
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    try {
        const auto args = app.arguments();
        require(args.size() >= 2,
                "Usage: export-branding <repository> [--verify-only] [--report <path>] "
                "[--window-preview]");
        repository = QDir(args[1]).absolutePath();
        const bool verifyOnly = args.contains("--verify-only");
        QString report = repository + "/verification/windows/branding/export-report.json";
        if (args.contains("--report")) {
            int i = args.indexOf("--report");
            require(i + 1 < args.size(), "Missing report path");
            report = args[i + 1];
        }
        require(!QFile::exists(report) || QFile::remove(report), "Cannot clear previous report");
        const QString branding = repository + "/resources/branding/";
        const QString media = repository + "/docs/media/branding/";
        const QString icons = repository + "/resources/icons/yalahoo";
        QJsonArray svgs;
        const QStringList files{ "icon-master.svg",     "icon.svg",
                                 "icon-small.svg",      "icon-mono.svg",
                                 "icon-mono-light.svg", "logo-master.svg",
                                 "logo-light.svg",      "logo-dark.svg",
                                 "logo-mono.svg",       "logo-mono-light.svg",
                                 "lockup-light.svg",    "lockup-dark.svg",
                                 "logo-reveal.svg" };
        for (const auto &file : files) {
            validateSvg(branding + file);
            QSvgRenderer renderer(branding + file);
            const QSize size = renderer.viewBox().size();
            const auto image = render(branding + file, size);
            const auto bounds = alphaBounds(image);
            require(!bounds.isEmpty() && !bounds.contains(QPoint(0, 0))
                            && image.pixelColor(0, 0).alpha() == 0
                            && image.pixelColor(size.width() - 1, size.height() - 1).alpha() == 0,
                    "SVG transparency/bounds invalid");
            if (file.startsWith("icon"))
                require(bounds.left() >= 96 && bounds.top() >= 96 && bounds.right() < 928
                                && bounds.bottom() < 928,
                        "Icon exceeds 96-unit safe area");
            if (file.startsWith("logo")) {
                require(renderer.elementExists("word") && renderer.elementExists("rings")
                                && renderer.elementExists("accents"),
                        "Missing stable logo groups");
                require(bounds.bottom() < 200, "Wordmark descender clipped");
            }
            svgs.append(QJsonObject{
                    { "path", "resources/branding/" + file },
                    { "sha256",
                      QString::fromLatin1(QCryptographicHash::hash(read(branding + file),
                                                                   QCryptographicHash::Sha256)
                                                  .toHex()) },
                    { "alphaBounds",
                      QJsonArray{ bounds.x(), bounds.y(), bounds.width(), bounds.height() } },
                    { "qtSvg", "passed" } });
        }
        require(samePixels(render(branding + "icon-master.svg", { 1024, 1024 }),
                           render(branding + "icon.svg", { 1024, 1024 })),
                "Icon production differs from master");
        require(samePixels(render(branding + "logo-master.svg", { 816, 208 }),
                           render(branding + "logo-light.svg", { 816, 208 })),
                "Logo production differs from master");
        const auto optical = render(branding + "icon-small.svg", { 16, 16 });
        const int holeOne = qGray(optical.pixel(6, 8));
        const int holeTwo = qGray(optical.pixel(10, 7));
        const int ringOne = qGray(optical.pixel(6, 7));
        const int ringTwo = qGray(optical.pixel(10, 6));
        const int gap = qGray(optical.pixel(8, 8));
        require(holeOne > 150 && holeTwo > 150 && holeOne - ringOne > 40 && holeTwo - ringTwo > 40
                        && gap > 180,
                "16px holes/ring separation lost");
        const auto mapping = revealMapping();
        const QString mappingPath = branding + "reveal-map.json";
        if (verifyOnly)
            require(QJsonDocument::fromJson(read(mappingPath)).object() == mapping,
                    "Reveal mapping stale");
        else
            write(mappingPath, QJsonDocument(mapping).toJson());
        QJsonArray previews;
        const QList<int> sizes{ 16, 24, 32, 48, 64, 128, 256, 512, 1024 };
        const QList<double> dprs{ 1, 1.5, 2 };
        for (int size : sizes) {
            const auto output = icons + QString("/png/yalahoo-%1.png").arg(size);
            const auto image = render(sourceForSize(size), { size, size });
            if (verifyOnly)
                require(samePixels(QImage(output), image), "PNG stale: " + output);
            else
                save(image, output);
            for (double dpr : dprs) {
                for (const auto &kind : QStringList{ "icon", "small", "mono" }) {
                    const QString source = branding
                            + (kind == "icon" ? QString(size <= 32 ? "icon-small.svg" : "icon.svg")
                                              : "icon-" + kind + ".svg");
                    const int pixels = int(std::lround(size * dpr));
                    const auto raster = render(source, { pixels, pixels });
                    const auto bounds = alphaBounds(raster);
                    require(!bounds.isEmpty() && bounds.left() > 0 && bounds.top() > 0
                                    && bounds.right() < pixels - 1 && bounds.bottom() < pixels - 1,
                            "Raster edge clipped");
                    for (const auto &theme : QStringList{ "transparent", "day", "night" }) {
                        const QString actualSource = kind == "mono" && theme == "night"
                                ? branding + "icon-mono-light.svg"
                                : source;
                        const QImage actualRaster = actualSource == source
                                ? raster
                                : render(actualSource, { pixels, pixels });
                        QImage preview = actualRaster;
                        if (theme != "transparent") {
                            preview.fill(QColor(theme == "day" ? "#e8e5e1" : "#08121f"));
                            QPainter p(&preview);
                            p.drawImage(0, 0, actualRaster);
                        }
                        const QString relative = QString("renders/%1-%2-dpr%3-%4.png")
                                                         .arg(kind)
                                                         .arg(size)
                                                         .arg(dpr)
                                                         .arg(theme);
                        if (!verifyOnly)
                            save(preview, media + relative);
                        previews.append(
                                QJsonObject{ { "source", QFileInfo(actualSource).fileName() },
                                             { "logicalSize", size },
                                             { "dpr", dpr },
                                             { "pixels", pixels },
                                             { "background", theme },
                                             { "preview", "docs/media/branding/" + relative } });
                    }
                }
            }
        }
        QJsonArray logoPreviews;
        for (const auto &theme : QStringList{ "light", "dark", "mono", "mono-light" }) {
            for (int cap : QList<int>{ 20, 24, 31 }) {
                for (double dpr : dprs) {
                    const QSize pixels(int(std::lround(816.0 * cap / 104 * dpr)),
                                       int(std::lround(208.0 * cap / 104 * dpr)));
                    const auto raster = render(branding + "logo-" + theme + ".svg", pixels);
                    const QString relative =
                            QString("logos/logo-%1-cap%2-dpr%3.png").arg(theme).arg(cap).arg(dpr);
                    if (!verifyOnly)
                        save(raster, media + relative);
                    logoPreviews.append(
                            QJsonObject{ { "source", "logo-" + theme + ".svg" },
                                         { "capHeight", cap },
                                         { "dpr", dpr },
                                         { "width", pixels.width() },
                                         { "height", pixels.height() },
                                         { "preview", "docs/media/branding/" + relative } });
                }
            }
        }
        for (const auto &theme : QStringList{ "light", "dark" }) {
            for (double width : QList<double>{ 155, 217.6, 235 }) {
                for (double dpr : dprs) {
                    const QSize pixels(int(std::lround(width * dpr)),
                                       int(std::lround(width * 145 / 310 * dpr)));
                    const auto raster = render(branding + "logo-" + theme + ".svg", pixels);
                    const QString relative = QString("logos/opening-%1-width%2-dpr%3.png")
                                                     .arg(theme)
                                                     .arg(width)
                                                     .arg(dpr);
                    if (!verifyOnly)
                        save(raster, media + relative);
                    logoPreviews.append(
                            QJsonObject{ { "source", "logo-" + theme + ".svg" },
                                         { "slotWidth", width },
                                         { "slotRatio", "310:145" },
                                         { "dpr", dpr },
                                         { "preview", "docs/media/branding/" + relative } });
                }
            }
        }
        const QMap<QString, int> iconset{
            { "icon_16x16.png", 16 },    { "icon_16x16@2x.png", 32 },
            { "icon_32x32.png", 32 },    { "icon_32x32@2x.png", 64 },
            { "icon_128x128.png", 128 }, { "icon_128x128@2x.png", 256 },
            { "icon_256x256.png", 256 }, { "icon_256x256@2x.png", 512 },
            { "icon_512x512.png", 512 }, { "icon_512x512@2x.png", 1024 }
        };
        for (auto it = iconset.begin(); it != iconset.end(); ++it) {
            const auto image = render(sourceForSize(it.value()), { it.value(), it.value() });
            const QString path = icons + "/yalahoo.iconset/" + it.key();
            if (verifyOnly)
                require(samePixels(QImage(path), image), "Iconset stale");
            else
                save(image, path);
        }
        if (!verifyOnly) {
            write(icons + "/yalahoo.svg", read(branding + "icon.svg"));
            exportContainers(icons);
        }
        require(read(icons + "/yalahoo.svg") == read(branding + "icon.svg"), "Linux SVG stale");
        const auto containers = verifyContainers(icons);
        QImage board(1440, 1060, QImage::Format_ARGB32_Premultiplied);
        board.fill(QColor("#e8e5e1"));
        QPainter painter(&board);
        painter.setRenderHint(QPainter::Antialiasing);
        QFont caption("Segoe UI", 12);
        painter.setFont(caption);
        painter.setPen(QColor("#74786f"));
        painter.drawText(40, 34,
                         "yalahOO!!!    /    ORIGINAL COLLECTION MARK    /    QT SVG PROOF");
        painter.fillRect(QRect(720, 56, 720, 420), QColor("#08121f"));
        QSvgRenderer light(branding + "lockup-light.svg"), dark(branding + "lockup-dark.svg");
        light.render(&painter, QRectF(40, 86, 630, 125));
        dark.render(&painter, QRectF(760, 86, 630, 125));
        painter.drawText(40, 258, "Header 31px lettering · 24px compact · 20px minimum");
        QSvgRenderer logo(branding + "logo-light.svg"), nightLogo(branding + "logo-dark.svg");
        // 104-unit capital height corresponds exactly to the frozen 31/24/20px line.
        for (int i = 0; i < 3; ++i) {
            const int cap = QList<int>{ 31, 24, 20 }[i];
            logo.render(&painter, QRectF(40, 275 + i * 60, 816.0 * cap / 104, 208.0 * cap / 104));
            nightLogo.render(&painter,
                             QRectF(760, 275 + i * 60, 816.0 * cap / 104, 208.0 * cap / 104));
        }
        painter.drawText(
                40, 525,
                "Optical small mark · native 16 / 24 / 32 / 48 / 64 / 128 pixels, both themes");
        int x = 40;
        for (int size : QList<int>{ 16, 24, 32, 48, 64, 128 }) {
            const auto image = render(sourceForSize(size), { size, size });
            painter.drawImage(x, 550, image);
            painter.fillRect(QRect(x, 700, size, size), QColor("#08121f"));
            painter.drawImage(x, 700, image);
            painter.drawText(x, 860, QString::number(size));
            x += size + 45;
        }
        painter.drawText(40, 915,
                         "Opening centerline progress · 0.25 / 0.5 / 0.75 / 1.0 · original "
                         "0.24–1.64s window");
        for (int i = 0; i < 4; ++i)
            paintReveal(painter, mapping, (i + 1) * .25, QRectF(40 + i * 345, 945, 320, 82));
        painter.end();
        if (!verifyOnly) {
            QImage optics(800, 340, QImage::Format_ARGB32_Premultiplied);
            optics.fill(QColor("#e8e5e1"));
            QPainter o(&optics);
            o.setRenderHint(QPainter::SmoothPixmapTransform, false);
            int opticalX = 20;
            for (int size : QList<int>{ 16, 24, 32 }) {
                const auto image = render(sourceForSize(size), { size, size });
                o.drawImage(QRect(opticalX, 40, 192, 192), image);
                o.drawImage(opticalX, 260, image);
                o.setPen(QColor("#74786f"));
                o.drawText(opticalX, 305,
                           QString("%1px · nearest-neighbor / actual size").arg(size));
                opticalX += 260;
            }
            o.end();
            save(optics, media + "small-optics.png");
            save(board, media + "qt-proof.png");
            validateSvg(media + "concepts.svg");
            save(render(media + "concepts.svg", { 1536, 860 }), media + "concepts.png");
            // Header/opening single-slot proof; fixed container, contain fit, no controls moved.
            QImage placement(1280, 720, QImage::Format_ARGB32_Premultiplied);
            placement.fill(QColor("#e8e5e1"));
            QPainter p(&placement);
            p.setRenderHint(QPainter::Antialiasing);
            logo.render(&p, QRectF(40, 33, 243.23, 62));
            logo.render(&p, QRectF(531.2, 280.2, 217.6, 101.78));
            p.setFont(caption);
            p.setPen(QColor("#74786f"));
            p.drawText(40, 140,
                       "Frozen header origin (40,33), 31px cap height; subtitle remains a separate "
                       "future UI node.");
            p.drawText(420, 440,
                       "Opening 1280px: clamp(155px,17vw,235px); 310:145 slot; 48% anchor.");
            p.end();
            save(placement, media + "placement-1280x720.png");
        }
        QJsonObject result{
            { "passed", true },
            { "qtVersion", qVersion() },
            { "operatingSystem", QSysInfo::prettyProductName() },
            { "cxxStandard", 23 },
            { "svgInventory", svgs },
            { "containers", containers },
            { "rasterCases", previews },
            { "logoCases", logoPreviews },
            { "revealSegments", mapping["paths"].toArray().size() },
            { "smallGeometry",
              QJsonObject{ { "borderUnits", 64 },
                           { "ringStrokeUnits", 64 },
                           { "ringHoleDiameterUnits", 64 },
                           { "ringGapUnits", 71.88 },
                           { "caseToRingGapUnits", 68.48 },
                           { "holeOneGray", holeOne },
                           { "holeTwoGray", holeTwo },
                           { "ringOneGray", ringOne },
                           { "ringTwoGray", ringTwo },
                           { "gapGray", gap },
                           { "dpr1At16", "1 logical pixel before -8 degree rotation" } } }
        };
        if (args.contains("--window-preview")) {
            require(!verifyOnly, "Window preview requires export mode");
            PreviewWindow window(board);
            window.show();
            QTimer::singleShot(100, &window, [&] {
                window.hide();
                window.show();
            });
            bool grabbed = false;
            QTimer::singleShot(750, &app, [&] {
                try {
                    require(window.isExposed(), "Qt proof window not exposed");
                    const QImage capture = window.screen()->grabWindow(window.winId()).toImage();
                    require(!capture.isNull(), "Native Qt window capture failed");
                    save(capture, media + "windows-window.png");
                    result["nativeWindow"] =
                            QJsonObject{ { "exposed", true },
                                         { "capture", "docs/media/branding/windows-window.png" },
                                         { "dpr", window.devicePixelRatio() },
                                         { "width", capture.width() },
                                         { "height", capture.height() },
                                         { "platform", QGuiApplication::platformName() } };
                    grabbed = true;
                } catch (const std::exception &e) {
                    qCritical("%s", e.what());
                }
                app.quit();
            });
            app.exec();
            require(grabbed, "Native Qt window verification failed");
        }
        if (verifyOnly) {
            const auto manifest =
                    QJsonDocument::fromJson(read(repository + "/resources/manifests/branding.json"))
                            .object();
            const auto entries = manifest["entries"].toArray();
            require(entries.size() == 36 && manifest["displayName"] == "yalahOO!!!",
                    "Brand manifest inventory invalid");
            for (const auto &value : entries) {
                const auto entry = value.toObject();
                const auto bytes = read(repository + "/" + entry["path"].toString());
                require(bytes.size() == entry["bytes"].toDouble()
                                && QString::fromLatin1(QCryptographicHash::hash(
                                                               bytes, QCryptographicHash::Sha256)
                                                               .toHex())
                                        == entry["sha256"].toString(),
                        "Brand manifest hash differs: " + entry["path"].toString());
            }
        }
        if (!verifyOnly) {
            QJsonArray entries;
            const QStringList trees{ "resources/branding", "resources/icons/yalahoo" };
            for (const auto &tree : trees) {
                std::function<void(const QString &)> visit = [&](const QString &relative) {
                    QDir current(repository + "/" + relative);
                    for (const auto &file : current.entryInfoList(
                                 QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
                        const QString path = relative + "/" + file.fileName();
                        if (file.isDir())
                            visit(path);
                        else
                            entries.append(QJsonObject{
                                    { "path", path },
                                    { "bytes", double(file.size()) },
                                    { "sha256",
                                      QString::fromLatin1(QCryptographicHash::hash(
                                                                  read(file.absoluteFilePath()),
                                                                  QCryptographicHash::Sha256)
                                                                  .toHex()) },
                                    { "source",
                                      tree.endsWith("branding")
                                              ? "Original yalahOO!!! geometric path design, R03B"
                                              : "resources/branding/icon.svg or icon-small.svg; "
                                                "QSvgRenderer derivative" } });
                    }
                };
                visit(tree);
            }
            write(repository + "/resources/manifests/branding.json",
                  QJsonDocument(QJsonObject{ { "schemaVersion", 1 },
                                             { "displayName", "yalahOO!!!" },
                                             { "authoring",
                                               "Original vector geometry; no upstream brand "
                                               "glyphs, external fonts or bitmap tracing" },
                                             { "qtVersion", qVersion() },
                                             { "entries", entries } })
                          .toJson());
        }
        write(report, QJsonDocument(result).toJson());
        qInfo("Branding verified: %lld SVGs, %lld raster cases, 7 ICO PNGs, 4 ICNS PNGs, 17 reveal "
              "segments",
              qint64(svgs.size()), qint64(previews.size()));
        return 0;
    } catch (const std::exception &e) {
        qCritical("%s", e.what());
        return 1;
    }
}
