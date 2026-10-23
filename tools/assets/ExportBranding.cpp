#include "../../src/render/opening/BrandPaths.h"
#include "BrandingManifest.h"
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
#include "VerificationWindow.h"
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

using namespace Qt::StringLiterals;

namespace {
// ICO/ICNS fields are binary container offsets and lengths, independent of the fixture icon sizes.
constexpr quint32 IcoHeaderBytes = 6;
constexpr quint32 IcoEntryBytes = 16;
constexpr quint32 IcoDirectoryBytes = 118;
constexpr int IcoEntryCount = 7;
constexpr int IcoMaximumDimension = 256;
constexpr int IcoColorBits = 32;
constexpr int IcoLengthOffset = 8;
constexpr int IcoPayloadOffset = 12;
constexpr int IcoPlanesOffset = 4;
constexpr int IcoColorBitsOffset = 6;
constexpr int IcnsHeaderBytes = 8;
constexpr int ContainerWordBytes = 4;
constexpr int NativeWindowPaintMilliseconds = 100;
constexpr int NativeWindowDeadlineMilliseconds = 750;

// Construct on first access; no namespace-scope runtime initialization is required.
QString &repository()
{
    static QString value;
    return value;
}
void require(bool condition, const QString &message)
{
    if (!condition)
        throw std::runtime_error(message.toStdString());
}
QByteArray read(const QString &path)
{
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Cannot read "_L1 + path);
    return file.readAll();
}
void write(const QString &path, const QByteArray &bytes)
{
    require(QDir().mkpath(QFileInfo(path).absolutePath()), u"Cannot create output directory"_s);
    QSaveFile file(path);
    require(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit(),
            "Cannot save "_L1 + path);
}
void save(const QImage &image, const QString &path)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    require(buffer.open(QIODevice::WriteOnly) && image.save(&buffer, "PNG"),
            u"PNG encoder failed"_s);
    write(path, bytes);
}
QImage render(const QString &path, QSize size)
{
    QSvgRenderer renderer(path);
    require(renderer.isValid(), "QtSvg rejected "_L1 + path);
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
bool sameDerivativePixels(const QImage &a, const QImage &b)
{
    if (a.isNull() || b.isNull() || a.size() != b.size())
        return false;
    const auto left = a.convertToFormat(QImage::Format_ARGB32);
    const auto right = b.convertToFormat(QImage::Format_ARGB32);
    // Native raster backends can differ by one RGB quantization unit. Alpha,
    // geometry, and the manifest's exact source/derivative hashes stay strict.
    for (int y = 0; y < left.height(); ++y) {
        const auto *first = reinterpret_cast<const QRgb *>(left.constScanLine(y));
        const auto *second = reinterpret_cast<const QRgb *>(right.constScanLine(y));
        for (int x = 0; x < left.width(); ++x) {
            if (qAlpha(first[x]) != qAlpha(second[x])
                || std::abs(qRed(first[x]) - qRed(second[x])) > 1
                || std::abs(qGreen(first[x]) - qGreen(second[x])) > 1
                || std::abs(qBlue(first[x]) - qBlue(second[x])) > 1)
                return false;
        }
    }
    return true;
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
    const QSet<QString> allowed{ u"svg"_s,     u"title"_s,          u"desc"_s,
                                 u"defs"_s,    u"linearGradient"_s, u"radialGradient"_s,
                                 u"stop"_s,    u"clipPath"_s,       u"g"_s,
                                 u"path"_s,    u"rect"_s,           u"circle"_s,
                                 u"ellipse"_s, u"line"_s,           u"polyline"_s,
                                 u"polygon"_s };
    QXmlStreamReader xml(read(path));
    QSet<QString> ids;
    QStringList references;
    bool viewBox = false;
    while (!xml.atEnd()) {
        xml.readNext();
        require(!xml.isDTD(), u"SVG DTD is not supported"_s);
        if (!xml.isStartElement())
            continue;
        require(allowed.contains(xml.name().toString()),
                "Unsupported SVG element: "_L1 + xml.name().toString());
        for (const auto &a : xml.attributes()) {
            const auto name = a.name().toString();
            const auto value = a.value().toString();
            require(!name.startsWith(u"on"_s) && name != "href"_L1 && name != "filter"_L1
                            && !value.contains(u"http:"_s) && !value.contains(u"https:"_s)
                            && !value.contains(u"data:"_s),
                    u"External or executable SVG attribute"_s);
            const QRegularExpression localReference(u"url\\(#([^)]*)\\)"_s);
            auto urls = localReference.globalMatch(value);
            while (urls.hasNext())
                references.append(urls.next().captured(1));
            if (name == "id"_L1) {
                require(!ids.contains(value), "Duplicate SVG id: "_L1 + value);
                ids.insert(value);
            }
            if (xml.name() == u"svg" && name == "viewBox"_L1)
                viewBox = true;
        }
    }
    for (const auto &reference : references)
        require(ids.contains(reference), "Missing local SVG reference: "_L1 + reference);
    require(!xml.hasError() && viewBox, "Invalid XML / missing viewBox: "_L1 + path);
    QSvgRenderer renderer(path);
    require(renderer.isValid() && !renderer.animated(), u"Invalid/animated SVG"_s);
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
    require(image.save(&buffer, "PNG"), u"PNG encoding failed"_s);
    return data;
}
QString sourceForSize(int size)
{
    return repository() + "/resources/branding/"_L1
            + (size <= 32 ? "icon-small.svg"_L1 : "icon.svg"_L1);
}
// Write each PNG block once, then encode directory lengths/offsets in the container byte order.
// ICO stores the largest dimension as zero; ICNS chunk lengths include their own type/length header.
void exportContainers(const QString &directory)
{
    const QList<int> sizes{ 16, 24, 32, 48, 64, 128, 256 };
    QByteArray ico;
    le16(ico, 0);
    le16(ico, 1);
    le16(ico, quint16(sizes.size()));
    QList<QByteArray> blocks;
    quint32 offset = IcoHeaderBytes + quint32(sizes.size()) * IcoEntryBytes;
    for (int size : sizes) {
        const auto block = png(render(sourceForSize(size), { size, size }));
        ico.append(char(size == IcoMaximumDimension ? 0 : size));
        ico.append(char(size == IcoMaximumDimension ? 0 : size));
        ico.append(char(0));
        ico.append(char(0));
        le16(ico, 1);
        le16(ico, IcoColorBits);
        le32(ico, quint32(block.size()));
        le32(ico, offset);
        offset += quint32(block.size());
        blocks.append(block);
    }
    for (const auto &block : blocks)
        ico.append(block);
    write(directory + "/yalahoo.ico"_L1, ico);
    QByteArray chunks;
    const QList<int> macSizes{ 128, 256, 512, 1024 };
    const QList<QByteArray> types{ "ic07"_ba, "ic08"_ba, "ic09"_ba, "ic10"_ba };
    for (int i = 0; i < macSizes.size(); ++i) {
        const auto block = png(render(sourceForSize(macSizes[i]), { macSizes[i], macSizes[i] }));
        chunks.append(types[i]);
        be32(chunks, quint32(block.size()) + IcnsHeaderBytes);
        chunks.append(block);
    }
    QByteArray icns("icns"_ba);
    be32(icns, quint32(chunks.size()) + IcnsHeaderBytes);
    icns.append(chunks);
    write(directory + "/yalahoo.icns"_L1, icns);
}
QJsonArray verifyContainers(const QString &directory)
{
    QJsonArray result;
    const auto ico = read(directory + "/yalahoo.ico"_L1);
    const auto bytes = reinterpret_cast<const uchar *>(ico.constData());
    require(ico.size() >= IcoDirectoryBytes && qFromLittleEndian<quint16>(bytes) == 0
                    && qFromLittleEndian<quint16>(bytes + 2) == 1
                    && qFromLittleEndian<quint16>(bytes + IcoPlanesOffset) == IcoEntryCount,
            u"ICO directory header invalid"_s);
    quint32 next = IcoDirectoryBytes;
    const QList<int> sizes{ 16, 24, 32, 48, 64, 128, 256 };
    for (int i = 0; i < IcoEntryCount; ++i) {
        const auto entry = bytes + IcoHeaderBytes + i * IcoEntryBytes;
        int size = entry[0] == 0 ? IcoMaximumDimension : entry[0];
        const auto length = qFromLittleEndian<quint32>(entry + IcoLengthOffset);
        const auto offset = qFromLittleEndian<quint32>(entry + IcoPayloadOffset);
        require(size == sizes[i] && entry[1] == entry[0] && entry[2] == 0 && entry[3] == 0
                        && qFromLittleEndian<quint16>(entry + IcoPlanesOffset) == 1
                        && qFromLittleEndian<quint16>(entry + IcoColorBitsOffset) == IcoColorBits
                        && offset == next && quint64(offset) + length <= quint64(ico.size()),
                u"ICO entry invalid"_s);
        const auto image = QImage::fromData(ico.mid(offset, length), "PNG");
        require(image.size() == QSize(size, size) && image.hasAlphaChannel(), u"ICO PNG invalid"_s);
        require(sameDerivativePixels(image, render(sourceForSize(size), { size, size })),
                u"ICO differs from current SVG"_s);
        next += length;
        result.append(QJsonObject{ { u"container"_s, u"ico"_s },
                                   { u"size"_s, size },
                                   { u"offset"_s, int(offset) },
                                   { u"bytes"_s, int(length) } });
    }
    require(next == ico.size(), u"ICO trailing bytes"_s);
    const auto icns = read(directory + "/yalahoo.icns"_L1);
    require(icns.size() >= IcnsHeaderBytes && icns.first(ContainerWordBytes) == "icns"_ba
                    && qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(
                               icns.constData() + ContainerWordBytes))
                            == icns.size(),
            u"ICNS header invalid"_s);
    qsizetype cursor = IcnsHeaderBytes;
    const QList<QByteArray> types{ "ic07"_ba, "ic08"_ba, "ic09"_ba, "ic10"_ba };
    const QList<int> macSizes{ 128, 256, 512, 1024 };
    for (int i = 0; i < 4; ++i) {
        require(cursor + IcnsHeaderBytes <= icns.size(), u"ICNS truncated header"_s);
        const auto type = icns.mid(cursor, ContainerWordBytes);
        const auto length = qFromBigEndian<quint32>(
                reinterpret_cast<const uchar *>(icns.constData() + cursor + ContainerWordBytes));
        require(type == types[i] && length >= IcnsHeaderBytes && cursor + length <= icns.size(),
                u"ICNS block invalid"_s);
        const auto image = QImage::fromData(
                icns.mid(cursor + IcnsHeaderBytes, length - IcnsHeaderBytes), "PNG");
        require(image.size() == QSize(macSizes[i], macSizes[i]) && image.hasAlphaChannel()
                        && sameDerivativePixels(
                                image,
                                render(sourceForSize(macSizes[i]), { macSizes[i], macSizes[i] })),
                u"ICNS PNG differs from current SVG"_s);
        result.append(QJsonObject{ { u"container"_s, u"icns"_s },
                                   { u"type"_s, QString::fromUtf8(type) },
                                   { u"size"_s, macSizes[i] },
                                   { u"bytes"_s, int(length) } });
        cursor += length;
    }
    require(cursor == icns.size(), u"ICNS unexpected chunks/trailing bytes"_s);
    return result;
}
// Only the actual authoring grammar is accepted: absolute M/L/Q/C/V/Z.
using yalahoo::parseBrandCenterline;
QJsonObject revealMapping()
{
    QXmlStreamReader xml(read(repository() + "/resources/branding/logo-reveal.svg"_L1));
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
        const auto path = parseBrandCenterline(d);
        const double length = path.length();
        require(!id.isEmpty() && length > 0, u"Empty reveal segment"_s);
        paths.append(QJsonObject{ { u"id"_s, id },
                                  { u"group"_s, group },
                                  { u"d"_s, d },
                                  { u"length"_s, length },
                                  { u"offset"_s, total },
                                  { u"strokeWidth"_s, group == "rings"_L1 ? 18 : 16 } });
        total += length;
    }
    require(!xml.hasError() && paths.size() == 17, u"Reveal segment inventory invalid"_s);
    for (int i = 0; i < paths.size(); ++i) {
        auto entry = paths[i].toObject();
        entry[u"start"_s] = entry[u"offset"_s].toDouble() / total;
        entry[u"end"_s] = (entry[u"offset"_s].toDouble() + entry[u"length"_s].toDouble()) / total;
        paths[i] = entry;
    }
    return {
        { u"schemaVersion"_s, 1 },
        { u"viewBox"_s, QJsonArray{ 0, 0, 816, 208 } },
        { u"totalLength"_s, total },
        { u"paths"_s, paths },
        { u"progressRule"_s,
          u"clamp((contourProgress-start)/(end-start),0,1); arc length, not Bezier parameter"_s },
        { u"clockRule"_s,
          u"musicOpeningFrame.contour: settle(unit(t,0.24,1.64)); accents also use original "
          "symbols envelope; no duration change"_s },
        { u"terminalRule"_s,
          u"reduced motion: show complete production logo; reveal at 1 replaces only inner "
          "groove details with full logo"_s }
    };
}
void paintReveal(QPainter &p, const QJsonObject &mapping, double progress, const QRectF &rect)
{
    p.save();
    p.translate(rect.topLeft());
    p.scale(rect.width() / 816, rect.height() / 208);
    for (const auto &value : mapping[u"paths"_s].toArray()) {
        const auto entry = value.toObject();
        const double t =
                std::clamp((progress - entry[u"start"_s].toDouble())
                                   / (entry[u"end"_s].toDouble() - entry[u"start"_s].toDouble()),
                           0.0, 1.0);
        if (t <= 0)
            continue;
        auto path = parseBrandCenterline(entry[u"d"_s].toString());
        const double length = path.length() * t;
        QPainterPath partial;
        partial.moveTo(path.pointAtPercent(0));
        const int steps = std::max(1, int(std::ceil(length / 0.5)));
        for (int step = 1; step <= steps; ++step)
            partial.lineTo(path.pointAtPercent(path.percentAtLength(length * step / steps)));
        QPen pen(QColor(entry[u"group"_s] == u"accents"_s ? "#b56834" : "#232722"),
                 entry[u"strokeWidth"_s].toDouble(), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.drawPath(partial);
    }
    p.restore();
}
class PreviewWindow final : public QWindow
{
    Q_DISABLE_COPY_MOVE(PreviewWindow)
    QBackingStore backing{ this };
    QImage board;
    QImage rendered;

public:
    explicit PreviewWindow(QImage image) : board(std::move(image))
    {
        yalahoo::verification::prepareWindow(*this);
        setTitle(u"yalahOO!!! \u00b7 R03B Qt brand proof"_s);
        resize(board.size());
    }
    QImage renderedImage() const
    {
        return rendered;
    }
    void exposeEvent(QExposeEvent *) override
    {
        if (!isExposed())
            return;
        rendered = QImage(size() * devicePixelRatio(), QImage::Format_ARGB32_Premultiplied);
        rendered.setDevicePixelRatio(devicePixelRatio());
        rendered.fill(Qt::transparent);
        QPainter imagePainter(&rendered);
        imagePainter.drawImage(QRect(QPoint(), size()), board);
        imagePainter.end();
        backing.resize(size());
        backing.beginPaint(QRect(QPoint(), size()));
        QPainter p(backing.paintDevice());
        p.drawImage(QPoint(), rendered);
        p.end();
        backing.endPaint();
        backing.flush(QRect(QPoint(), size()));
    }
};

auto makeBrandPreview(PreviewWindow &window, const QString &media, QJsonObject &result,
                      bool &grabbed, QGuiApplication &app)
{
    try {
        require(window.isExposed(), u"Qt proof window not exposed"_s);
        require(yalahoo::verification::windowPolicySatisfied(window),
                u"Brand proof window violated bottom/no-focus policy"_s);
        // Bottom windows can be occluded by the user's work. Capture only our own
        // rendered board; never sample the desktop or raise a proof window.
        const QImage capture = window.renderedImage();
        require(!capture.isNull(), u"Native Qt window capture failed"_s);
        save(capture, media + "windows-window.png"_L1);
        result[u"nativeWindow"_s] =
                QJsonObject{ { u"exposed"_s, true },
                             { u"capture"_s, u"docs/media/branding/windows-window.png"_s },
                             { u"captureMethod"_s, u"own rendered board; no desktop capture"_s },
                             { u"windowPolicy"_s, yalahoo::verification::windowPolicy(window) },
                             { u"dpr"_s, window.devicePixelRatio() },
                             { u"width"_s, capture.width() },
                             { u"height"_s, capture.height() },
                             { u"platform"_s, QGuiApplication::platformName() } };
        grabbed = true;
    } catch (const std::exception &e) {
        qCritical("%s", e.what());
    }
    app.quit();
}

auto writeBrandAssets(std::function<void(const QString &)> &visit, QJsonArray &entries,
                      QString const &tree, const QString &relative)
{
    QDir current(repository() + "/"_L1 + relative);
    for (const auto &file :
         current.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        const QString path = relative + "/"_L1 + file.fileName();
        if (file.isDir())
            visit(path);
        else
            entries.append(QJsonObject{
                    { u"path"_s, path },
                    { u"bytes"_s, double(file.size()) },
                    { u"sha256"_s,
                      QString::fromUtf8(QCryptographicHash::hash(read(file.absoluteFilePath()),
                                                                 QCryptographicHash::Sha256)
                                                .toHex()) },
                    { u"source"_s,
                      tree.endsWith(u"branding"_s)
                              ? u"Original yalahOO!!! geometric path design, R03B"_s
                              : u"resources/branding/icon.svg or icon-small.svg; "
                                "QSvgRenderer derivative"_s } });
    }
}
}

int main(int argc, char **argv)
{
    yalahoo::verification::prepareApplication();
    QGuiApplication app(argc, argv);
    try {
        const auto args = app.arguments();
        require(args.size() >= 2,
                u"Usage: export-branding <repository> [--verify-only] [--report <path>] "
                "[--window-preview]"_s);
        repository() = QDir(args[1]).absolutePath();
        const bool verifyOnly = args.contains(u"--verify-only"_s);
        QString report = repository() + "/verification/windows/branding/export-report.json"_L1;
        if (args.contains(u"--report"_s)) {
            int i = args.indexOf(u"--report"_s);
            require(i + 1 < args.size(), u"Missing report path"_s);
            report = args[i + 1];
        }
        require(!QFile::exists(report) || QFile::remove(report), u"Cannot clear previous report"_s);
        const QString branding = repository() + "/resources/branding/"_L1;
        const QString media = repository() + "/docs/media/branding/"_L1;
        const QString icons = repository() + "/resources/icons/yalahoo"_L1;
        QJsonArray svgs;
        const QStringList files{ u"icon-master.svg"_s,     u"icon.svg"_s,
                                 u"icon-small.svg"_s,      u"icon-mono.svg"_s,
                                 u"icon-mono-light.svg"_s, u"logo-master.svg"_s,
                                 u"logo-light.svg"_s,      u"logo-dark.svg"_s,
                                 u"logo-mono.svg"_s,       u"logo-mono-light.svg"_s,
                                 u"lockup-light.svg"_s,    u"lockup-dark.svg"_s,
                                 u"logo-reveal.svg"_s };
        for (const auto &file : files) {
            validateSvg(branding + file);
            QSvgRenderer renderer(branding + file);
            const QSize size = renderer.viewBox().size();
            const auto image = render(branding + file, size);
            const auto bounds = alphaBounds(image);
            require(!bounds.isEmpty() && !bounds.contains(QPoint(0, 0))
                            && image.pixelColor(0, 0).alpha() == 0
                            && image.pixelColor(size.width() - 1, size.height() - 1).alpha() == 0,
                    u"SVG transparency/bounds invalid"_s);
            if (file.startsWith(u"icon"_s))
                require(bounds.left() >= 96 && bounds.top() >= 96 && bounds.right() < 928
                                && bounds.bottom() < 928,
                        u"Icon exceeds 96-unit safe area"_s);
            if (file.startsWith(u"logo"_s)) {
                require(renderer.elementExists(u"word"_s) && renderer.elementExists(u"rings"_s)
                                && renderer.elementExists(u"accents"_s),
                        u"Missing stable logo groups"_s);
                require(bounds.bottom() < 200, u"Wordmark descender clipped"_s);
            }
            svgs.append(QJsonObject{
                    { u"path"_s, "resources/branding/"_L1 + file },
                    { u"sha256"_s,
                      QString::fromUtf8(QCryptographicHash::hash(read(branding + file),
                                                                 QCryptographicHash::Sha256)
                                                .toHex()) },
                    { u"alphaBounds"_s,
                      QJsonArray{ bounds.x(), bounds.y(), bounds.width(), bounds.height() } },
                    { u"qtSvg"_s, u"passed"_s } });
        }
        require(samePixels(render(branding + "icon-master.svg"_L1, { 1024, 1024 }),
                           render(branding + "icon.svg"_L1, { 1024, 1024 })),
                u"Icon production differs from master"_s);
        require(samePixels(render(branding + "logo-master.svg"_L1, { 816, 208 }),
                           render(branding + "logo-light.svg"_L1, { 816, 208 })),
                u"Logo production differs from master"_s);
        const auto optical = render(branding + "icon-small.svg"_L1, { 16, 16 });
        const int holeOne = qGray(optical.pixel(6, 8));
        const int holeTwo = qGray(optical.pixel(10, 7));
        const int ringOne = qGray(optical.pixel(6, 7));
        const int ringTwo = qGray(optical.pixel(10, 6));
        const int gap = qGray(optical.pixel(8, 8));
        require(holeOne > 150 && holeTwo > 150 && holeOne - ringOne > 40 && holeTwo - ringTwo > 40
                        && gap > 180,
                u"16px holes/ring separation lost"_s);
        const auto mapping = revealMapping();
        const QString mappingPath = branding + "reveal-map.json"_L1;
        if (verifyOnly)
            require(QJsonDocument::fromJson(read(mappingPath)).object() == mapping,
                    u"Reveal mapping stale"_s);
        else
            write(mappingPath, QJsonDocument(mapping).toJson());
        QJsonArray previews;
        const QList<int> sizes{ 16, 24, 32, 48, 64, 128, 256, 512, 1024 };
        const QList<double> dprs{ 1, 1.5, 2 };
        for (int size : sizes) {
            const auto output = icons + u"/png/yalahoo-%1.png"_s.arg(size);
            const auto image = render(sourceForSize(size), { size, size });
            if (verifyOnly)
                require(sameDerivativePixels(QImage(output), image), "PNG stale: "_L1 + output);
            else
                save(image, output);
            for (double dpr : dprs) {
                for (const auto &kind : QStringList{ u"icon"_s, u"small"_s, u"mono"_s }) {
                    const QString source = branding
                            + (kind == "icon"_L1
                                       ? QString(size <= 32 ? u"icon-small.svg"_s : u"icon.svg"_s)
                                       : "icon-"_L1 + kind + ".svg"_L1);
                    const int pixels = int(std::lround(size * dpr));
                    const auto raster = render(source, { pixels, pixels });
                    const auto bounds = alphaBounds(raster);
                    require(!bounds.isEmpty() && bounds.left() > 0 && bounds.top() > 0
                                    && bounds.right() < pixels - 1 && bounds.bottom() < pixels - 1,
                            u"Raster edge clipped"_s);
                    for (const auto &theme :
                         QStringList{ u"transparent"_s, u"day"_s, u"night"_s }) {
                        const QString actualSource = kind == "mono"_L1 && theme == "night"_L1
                                ? branding + "icon-mono-light.svg"_L1
                                : source;
                        const QImage actualRaster = actualSource == source
                                ? raster
                                : render(actualSource, { pixels, pixels });
                        QImage preview = actualRaster;
                        if (theme != "transparent"_L1) {
                            preview.fill(QColor(theme == "day"_L1 ? "#e8e5e1" : "#08121f"));
                            QPainter p(&preview);
                            p.drawImage(0, 0, actualRaster);
                        }
                        const QString relative =
                                u"renders/%1-%2-dpr%3-%4.png"_s.arg(kind).arg(size).arg(dpr).arg(
                                        theme);
                        if (!verifyOnly)
                            save(preview, media + relative);
                        previews.append(QJsonObject{
                                { u"source"_s, QFileInfo(actualSource).fileName() },
                                { u"logicalSize"_s, size },
                                { u"dpr"_s, dpr },
                                { u"pixels"_s, pixels },
                                { u"background"_s, theme },
                                { u"preview"_s, "docs/media/branding/"_L1 + relative } });
                    }
                }
            }
        }
        QJsonArray logoPreviews;
        for (const auto &theme : QStringList{ u"light"_s, u"dark"_s, u"mono"_s, u"mono-light"_s }) {
            for (int cap : QList<int>{ 20, 24, 31 }) {
                for (double dpr : dprs) {
                    const QSize pixels(int(std::lround(816.0 * cap / 104 * dpr)),
                                       int(std::lround(208.0 * cap / 104 * dpr)));
                    const auto raster = render(branding + "logo-"_L1 + theme + ".svg"_L1, pixels);
                    const QString relative =
                            u"logos/logo-%1-cap%2-dpr%3.png"_s.arg(theme).arg(cap).arg(dpr);
                    if (!verifyOnly)
                        save(raster, media + relative);
                    logoPreviews.append(
                            QJsonObject{ { u"source"_s, "logo-"_L1 + theme + ".svg"_L1 },
                                         { u"capHeight"_s, cap },
                                         { u"dpr"_s, dpr },
                                         { u"width"_s, pixels.width() },
                                         { u"height"_s, pixels.height() },
                                         { u"preview"_s, "docs/media/branding/"_L1 + relative } });
                }
            }
        }
        for (const auto &theme : QStringList{ u"light"_s, u"dark"_s }) {
            for (double width : QList<double>{ 155, 217.6, 235 }) {
                for (double dpr : dprs) {
                    const QSize pixels(int(std::lround(width * dpr)),
                                       int(std::lround(width * 145 / 310 * dpr)));
                    const auto raster = render(branding + "logo-"_L1 + theme + ".svg"_L1, pixels);
                    const QString relative =
                            u"logos/opening-%1-width%2-dpr%3.png"_s.arg(theme).arg(width).arg(dpr);
                    if (!verifyOnly)
                        save(raster, media + relative);
                    logoPreviews.append(
                            QJsonObject{ { u"source"_s, "logo-"_L1 + theme + ".svg"_L1 },
                                         { u"slotWidth"_s, width },
                                         { u"slotRatio"_s, u"310:145"_s },
                                         { u"dpr"_s, dpr },
                                         { u"preview"_s, "docs/media/branding/"_L1 + relative } });
                }
            }
        }
        const QMap<QString, int> iconset{
            { u"icon_16x16.png"_s, 16 },    { u"icon_16x16@2x.png"_s, 32 },
            { u"icon_32x32.png"_s, 32 },    { u"icon_32x32@2x.png"_s, 64 },
            { u"icon_128x128.png"_s, 128 }, { u"icon_128x128@2x.png"_s, 256 },
            { u"icon_256x256.png"_s, 256 }, { u"icon_256x256@2x.png"_s, 512 },
            { u"icon_512x512.png"_s, 512 }, { u"icon_512x512@2x.png"_s, 1024 }
        };
        for (auto it = iconset.begin(); it != iconset.end(); ++it) {
            const auto image = render(sourceForSize(it.value()), { it.value(), it.value() });
            const QString path = icons + "/yalahoo.iconset/"_L1 + it.key();
            if (verifyOnly)
                require(sameDerivativePixels(QImage(path), image), u"Iconset stale"_s);
            else
                save(image, path);
        }
        if (!verifyOnly) {
            write(icons + "/yalahoo.svg"_L1, read(branding + "icon.svg"_L1));
            exportContainers(icons);
        }
        require(read(icons + "/yalahoo.svg"_L1) == read(branding + "icon.svg"_L1),
                u"Linux SVG stale"_s);
        const auto containers = verifyContainers(icons);
        QImage board(1440, 1060, QImage::Format_ARGB32_Premultiplied);
        board.fill(QColor("#e8e5e1"_L1));
        QPainter painter(&board);
        painter.setRenderHint(QPainter::Antialiasing);
        QFont caption(u"Segoe UI"_s, 12);
        painter.setFont(caption);
        painter.setPen(QColor("#74786f"_L1));
        painter.drawText(40, 34,
                         u"yalahOO!!!    /    ORIGINAL COLLECTION MARK    /    QT SVG PROOF"_s);
        painter.fillRect(QRect(720, 56, 720, 420), QColor("#08121f"_L1));
        QSvgRenderer light(branding + "lockup-light.svg"_L1), dark(branding + "lockup-dark.svg"_L1);
        light.render(&painter, QRectF(40, 86, 630, 125));
        dark.render(&painter, QRectF(760, 86, 630, 125));
        painter.drawText(40, 258,
                         u"Header 31px lettering \u00b7 24px compact \u00b7 20px minimum"_s);
        QSvgRenderer logo(branding + "logo-light.svg"_L1), nightLogo(branding + "logo-dark.svg"_L1);
        // 104-unit capital height corresponds exactly to the frozen 31/24/20px line.
        for (int i = 0; i < 3; ++i) {
            const int cap = QList<int>{ 31, 24, 20 }[i];
            logo.render(&painter, QRectF(40, 275 + i * 60, 816.0 * cap / 104, 208.0 * cap / 104));
            nightLogo.render(&painter,
                             QRectF(760, 275 + i * 60, 816.0 * cap / 104, 208.0 * cap / 104));
        }
        painter.drawText(
                40, 525,
                u"Optical small mark \u00b7 native 16 / 24 / 32 / 48 / 64 / 128 pixels, both themes"_s);
        int x = 40;
        for (int size : QList<int>{ 16, 24, 32, 48, 64, 128 }) {
            const auto image = render(sourceForSize(size), { size, size });
            painter.drawImage(x, 550, image);
            painter.fillRect(QRect(x, 700, size, size), QColor("#08121f"_L1));
            painter.drawImage(x, 700, image);
            painter.drawText(x, 860, QString::number(size));
            x += size + 45;
        }
        painter.drawText(
                40, 915,
                u"Opening centerline progress \u00b7 0.25 / 0.5 / 0.75 / 1.0 \u00b7 original "
                "0.24\u20131.64s window"_s);
        for (int i = 0; i < 4; ++i)
            paintReveal(painter, mapping, (i + 1) * .25, QRectF(40 + i * 345, 945, 320, 82));
        painter.end();
        if (!verifyOnly) {
            QImage optics(800, 340, QImage::Format_ARGB32_Premultiplied);
            optics.fill(QColor("#e8e5e1"_L1));
            QPainter o(&optics);
            o.setRenderHint(QPainter::SmoothPixmapTransform, false);
            int opticalX = 20;
            for (int size : QList<int>{ 16, 24, 32 }) {
                const auto image = render(sourceForSize(size), { size, size });
                o.drawImage(QRect(opticalX, 40, 192, 192), image);
                o.drawImage(opticalX, 260, image);
                o.setPen(QColor("#74786f"_L1));
                o.drawText(opticalX, 305,
                           u"%1px \u00b7 nearest-neighbor / actual size"_s.arg(size));
                opticalX += 260;
            }
            o.end();
            save(optics, media + "small-optics.png"_L1);
            save(board, media + "qt-proof.png"_L1);
            validateSvg(media + "concepts.svg"_L1);
            save(render(media + "concepts.svg"_L1, { 1536, 860 }), media + "concepts.png"_L1);
            // Header/opening single-slot proof; fixed container, contain fit, no controls moved.
            QImage placement(1280, 720, QImage::Format_ARGB32_Premultiplied);
            placement.fill(QColor("#e8e5e1"_L1));
            QPainter p(&placement);
            p.setRenderHint(QPainter::Antialiasing);
            logo.render(&p, QRectF(40, 33, 243.23, 62));
            logo.render(&p, QRectF(531.2, 280.2, 217.6, 101.78));
            p.setFont(caption);
            p.setPen(QColor("#74786f"_L1));
            p.drawText(
                    40, 140,
                    u"Frozen header origin (40,33), 31px cap height; subtitle remains a separate "
                    "future UI node."_s);
            p.drawText(420, 440,
                       u"Opening 1280px: clamp(155px,17vw,235px); 310:145 slot; 48% anchor."_s);
            p.end();
            save(placement, media + "placement-1280x720.png"_L1);
        }
        QJsonObject result{
            { u"passed"_s, true },
            { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
            { u"operatingSystem"_s, QSysInfo::prettyProductName() },
            { u"cxxStandard"_s, 23 },
            { u"svgInventory"_s, svgs },
            { u"containers"_s, containers },
            { u"rasterCases"_s, previews },
            { u"logoCases"_s, logoPreviews },
            { u"revealSegments"_s, mapping[u"paths"_s].toArray().size() },
            { u"smallGeometry"_s,
              QJsonObject{ { u"borderUnits"_s, 64 },
                           { u"ringStrokeUnits"_s, 64 },
                           { u"ringHoleDiameterUnits"_s, 64 },
                           { u"ringGapUnits"_s, 71.88 },
                           { u"caseToRingGapUnits"_s, 68.48 },
                           { u"holeOneGray"_s, holeOne },
                           { u"holeTwoGray"_s, holeTwo },
                           { u"ringOneGray"_s, ringOne },
                           { u"ringTwoGray"_s, ringTwo },
                           { u"gapGray"_s, gap },
                           { u"dpr1At16"_s, u"1 logical pixel before -8 degree rotation"_s } } }
        };
        if (args.contains(u"--window-preview"_s)) {
            require(!verifyOnly, u"Window preview requires export mode"_s);
            PreviewWindow window(board);
            window.show();
            QTimer::singleShot(NativeWindowPaintMilliseconds, &window, [&] {
                window.hide();
                window.show();
            });
            bool grabbed = false;
            QTimer::singleShot(NativeWindowDeadlineMilliseconds, &app, [&] {
                return makeBrandPreview(window, media, result, grabbed, app);
            });
            app.exec();
            require(grabbed, u"Native Qt window verification failed"_s);
        }
        if (verifyOnly) {
            yalahoo::assets::validateBrandingManifest(
                    QDir(repository()),
                    QJsonDocument::fromJson(
                            read(repository() + "/resources/manifests/branding.json"_L1)));
        }
        if (!verifyOnly) {
            QJsonArray entries;
            const QStringList trees{ u"resources/branding"_s, u"resources/icons/yalahoo"_s };
            for (const auto &tree : trees) {
                std::function<void(const QString &)> visit = [&](const QString &relative) {
                    return writeBrandAssets(visit, entries, tree, relative);
                };
                visit(tree);
            }
            write(repository() + "/resources/manifests/branding.json"_L1,
                  QJsonDocument(QJsonObject{ { u"schemaVersion"_s, 1 },
                                             { u"displayName"_s, u"yalahOO!!!"_s },
                                             { u"authoring"_s,
                                               u"Original vector geometry; no upstream brand "
                                               "glyphs, external fonts or bitmap tracing"_s },
                                             { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
                                             { u"entries"_s, entries } })
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
