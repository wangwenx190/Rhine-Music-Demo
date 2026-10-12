#include "ImageComparison.h"
#include <QGuiApplication>
#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QPainter>
#include <QCryptographicHash>
#include <QSGRendererInterface>
namespace {
QJsonObject read(const QString &name)
{
    QFile f(name);
    if (!f.open(QIODevice::ReadOnly))
        return { };
    return QJsonDocument::fromJson(f.readAll()).object();
}
QJsonObject change(const QImage &a, const QImage &b)
{
    if (a.isNull() || a.size() != b.size())
        return { };
    int pixels = 0, peak = 0;
    quint64 sum = 0;
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x) {
            const auto p = a.pixelColor(x, y), q = b.pixelColor(x, y);
            const int d = std::abs(p.red() - q.red()) + std::abs(p.green() - q.green())
                    + std::abs(p.blue() - q.blue());
            pixels += d > 3;
            peak = std::max(peak, d);
            sum += d;
        }
    return { { "changedPixels", pixels },
             { "peakRgbSum", peak },
             { "meanAbsoluteRgb", double(sum) / (3 * a.width() * a.height()) } };
}
bool strip(const QString &path, const QImage &web, const QImage &qt, const QJsonObject &metric)
{
    QPolygonF polygon;
    for (auto v : metric["polygon"].toArray()) {
        const auto p = v.toArray();
        polygon << QPointF(p[0].toDouble(), p[1].toDouble());
    }
    const auto rect =
            polygon.boundingRect().toAlignedRect().adjusted(-3, -3, 3, 3).intersected(qt.rect());
    if (rect.isEmpty())
        return false;
    constexpr int scale = 4;
    QImage out(rect.width() * 3 * scale, rect.height() * scale + 26, QImage::Format_RGB32);
    out.fill(Qt::white);
    QPainter painter(&out);
    const QList<QImage> images{ web, qt, yalahoo::absoluteDifference(qt, web) };
    const QStringList labels{ "Frozen source", "Qt D3D11", "Absolute RGB diff" };
    for (int i = 0; i < 3; ++i) {
        painter.drawImage(
                QRect(i * rect.width() * scale, 26, rect.width() * scale, rect.height() * scale),
                images[i], rect);
        painter.drawText(i * rect.width() * scale + 4, 19, labels[i]);
    }
    painter.end();
    return out.save(path);
}
}
int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() != 5)
        return 2;
    QFile input(args[1]);
    if (!input.open(QIODevice::ReadOnly))
        return 2;
    const auto bytes = input.readAll();
    const auto fixture = QJsonDocument::fromJson(bytes).object(),
               runtime = read(args[2] + "/runtime.json");
    const auto cases = fixture["cases"].toArray(), results = runtime["cases"].toArray();
    bool passed = fixture["renderBackend"].toBool() && cases.size() == 70
            && results.size() == cases.size() && runtime["passed"].toBool()
            && runtime["fixtureSha256"].toString()
                    == QString::fromLatin1(
                            QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())
            && runtime["dpr"].toDouble() == 1;
#ifdef Q_OS_WIN
    passed = passed && runtime["graphicsApi"].toInt() == int(QSGRendererInterface::Direct3D11);
#endif
    if (results.size() != cases.size())
        return 3;
    QList<QImage> web, qt;
    QJsonArray residuals, losses, shadows;
    double maxProjection = 0;
    int depthNormalCases = 0;
    for (int i = 0; i < cases.size(); ++i) {
        const auto c = cases[i].toObject(), r = results[i].toObject();
        web.append(QImage(args[3] + "/" + c["screenshot"].toString()));
        qt.append(QImage(args[2] + "/" + c["screenshot"].toString()));
        passed = passed && !web.last().isNull() && web.last().size() == qt.last().size()
                && r["passed"].toBool() && r["frames"].toInt() >= 4
                && r["sourceParameterMatch"].toBool();
        maxProjection = std::max(maxProjection, r["webProjectionPixels"].toDouble());
        passed = passed && qint64(r["targetWidth"].toInt()) * r["targetHeight"].toInt() <= 8294400;
        const auto roi = r["roi"].toObject();
        double loss = 0;
        for (auto name : { "paper", "margin", "spine", "rim", "background" }) {
            const auto m = roi[name].toObject();
            passed = passed && m["valid"].toBool() && m["pixels"].toInt() > 0;
            loss += m["meanAbsoluteRgb"].toDouble() / 5;
            if (!m["edgeDistancePixels"].isNull())
                loss += m["edgeDistancePixels"].toDouble() / (5 * 255);
            if (m["meanAbsoluteRgb"].toDouble() > 6
                || (m["ssimWindows"].toInt() > 0 && m["ssim"].toDouble() < .97))
                residuals.append(QJsonObject{ { "index", i },
                                              { "group", c["group"] },
                                              { "sourceCase", c["sourceCase"] },
                                              { "region", name },
                                              { "metric", m } });
        }
        losses.append(QJsonObject{ { "index", i }, { "equalWeightLoss", loss } });
        if (i == 58 || i == 59) {
            ++depthNormalCases;
            passed = passed && roi["full"].toObject()["meanAbsoluteRgb"].toDouble() < .05;
        }
    }
    for (int i = 9; i < 21; i += 2) {
        const auto qw = change(qt[i], qt[i + 1]), ww = change(web[i], web[i + 1]);
        passed = passed && qw["changedPixels"].toInt() > 100 && ww["changedPixels"].toInt() > 100;
        shadows.append(QJsonObject{ { "day", cases[i].toObject()["day"] },
                                    { "resolution", cases[i].toObject()["shadowSize"] },
                                    { "qt", qw },
                                    { "source", ww } });
    }
    QJsonArray sorting;
    for (auto pair : { QPair{ 62, 63 }, QPair{ 64, 65 }, QPair{ 62, 64 } })
        sorting.append(QJsonObject{ { "first", pair.first },
                                    { "second", pair.second },
                                    { "qt", change(qt[pair.first], qt[pair.second]) },
                                    { "source", change(web[pair.first], web[pair.second]) } });
    const auto atlasChange = change(qt[66], qt[67]), atlasStable = change(qt[67], qt[68]),
               atlasReturn = change(qt[66], qt[69]);
    const auto update = results[67].toObject(), steady = results[68].toObject();
    passed = passed && atlasChange["changedPixels"].toInt() > 100
            && atlasStable["changedPixels"].toInt() == 0
            && atlasReturn["changedPixels"].toInt() == 0;
    const auto atlas = QJsonObject{
        { "changedPixels", atlasChange },
        { "stablePixels", atlasStable },
        { "returnPixels", atlasReturn },
        { "modifiedTileBytes", update["atlasModifiedTileBytes"] },
        { "actualD3D11UpdateBytes", update["atlasGpuUploadBytes"] },
        { "cpuPayloadBytes", update["atlasCpuPayloadBytes"] },
        { "stableCpuSubmissions",
          steady["atlasSubmissions"].toInt() - update["atlasSubmissions"].toInt() },
        { "method",
          "Actual GPU readback proves update/stable/return pixels. TextureData accepts full-page "
          "payload; actual GPU transfer bytes are unmeasured and must be verified in R10C." }
    };
    // Structural witnesses are separate from success of the probe infrastructure.
    passed = passed && maxProjection <= 2 && depthNormalCases == 2 && !residuals.isEmpty();
    for (auto item : { QPair{ 2, QString("background") }, QPair{ 3, QString("background") },
                       QPair{ 7, QString("margin") }, QPair{ 4, QString("margin") } })
        passed = passed
                && strip(args[2] + QString("/roi-%1-%2.png").arg(item.first).arg(item.second),
                         web[item.first], qt[item.first],
                         results[item.first].toObject()["roi"].toObject()[item.second].toObject());
    QSaveFile output(args[4]);
    if (!output.open(QIODevice::WriteOnly))
        return 4;
    output.write(QJsonDocument(QJsonObject{ { "passed", passed },
                                            { "parityPassed", false },
                                            { "selectedBackend", "rhi" },
                                            { "implementationReady", false },
                                            { "cases", cases.size() },
                                            { "fixtureSha256", runtime["fixtureSha256"] },
                                            { "maxProjectionPixels", maxProjection },
                                            { "depthNormalCases", depthNormalCases },
                                            { "equalWeightLoss", losses },
                                            { "residuals", residuals },
                                            { "shadowPairs", shadows },
                                            { "sorting", sorting },
                                            { "atlas", atlas } })
                         .toJson());
    return output.commit() && passed ? 0 : 5;
}
