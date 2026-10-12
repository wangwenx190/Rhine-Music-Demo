#include <QCoreApplication>
#include <QImage>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QSGRendererInterface>
#include <cmath>
#include <algorithm>
namespace {
struct Stats
{
    int minimum = 255, maximum = 0, belowWhite = 0;
    double mean = 0;
};
Stats stats(const QImage &image)
{
    Stats s;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) {
            int v = image.pixelColor(x, y).red();
            s.minimum = std::min(s.minimum, v);
            s.maximum = std::max(s.maximum, v);
            s.mean += v;
            s.belowWhite += v < 254;
        }
    if (!image.isNull())
        s.mean /= double(image.width()) * image.height();
    return s;
}
QJsonObject pair(const QImage &off, const QImage &on)
{
    int changed = 0, maxDifference = 0;
    double error = 0;
    if (off.isNull() || off.size() != on.size())
        return { };
    for (int y = 0; y < off.height(); ++y)
        for (int x = 0; x < off.width(); ++x) {
            int d = std::abs(off.pixelColor(x, y).red() - on.pixelColor(x, y).red());
            changed += d > 1;
            maxDifference = std::max(maxDifference, d);
            error += d;
        }
    return { { "changedPixels", changed },
             { "maxDifference", maxDifference },
             { "meanDifference", error / (off.width() * off.height()) } };
}
}
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() != 5)
        return 2;
    QFile file(args[1]);
    if (!file.open(QIODevice::ReadOnly))
        return 2;
    const auto report = QJsonDocument::fromJson(file.readAll()).object();
#if defined(Q_OS_WIN)
    constexpr auto expectedApi = QSGRendererInterface::Direct3D11;
#elif defined(Q_OS_MACOS)
    constexpr auto expectedApi = QSGRendererInterface::Metal;
#else
    constexpr auto expectedApi = QSGRendererInterface::OpenGL;
#endif
    const auto cases = report["cases"].toArray();
    bool passed = report["passed"].toBool() && cases.size() == 88 && report["dpr"].toDouble() == 1
            && report["graphicsApi"].toInt() == int(expectedApi);
    QJsonArray channels;
    double maxGeometryError = 0, maxAoError = 0, maxSingleMae = 0;
    double minSingleSsim = 1;
    for (const auto value : cases) {
        const auto c = value.toObject();
        const int index = c["index"].toInt(), channel = c["channel"].toInt();
        const QImage actual(args[2] + "/" + c["screenshot"].toString()),
                expected(args[3] + "/" + c["screenshot"].toString());
        passed = passed && c["passed"].toBool() && !actual.isNull() && !expected.isNull()
                && actual.size() == QSize(960, 720) && expected.size() == actual.size();
        if (channel == 3 || channel == 4) {
            maxGeometryError =
                    std::max(maxGeometryError,
                             c["roi"].toObject()["full"].toObject()["meanAbsoluteRgb"].toDouble());
        }
        // Single-case contract remains strict; overlap limitations are handed to R10.
        if (channel == 0 && (index < 20 || (index >= 40 && index < 60))) {
            // Background's OutputPass ACES conversion belongs to R09B. Keep
            // its full per-case metric, and gate all four material regions.
            for (auto name : { "paper", "margin", "spine", "rim" }) {
                const auto r = c["roi"].toObject()[name].toObject();
                maxSingleMae = std::max(maxSingleMae, r["meanAbsoluteRgb"].toDouble());
                if (r["ssimWindows"].toInt() > 0)
                    minSingleSsim = std::min(minSingleSsim, r["ssim"].toDouble());
            }
        }
        if (channel == 1 || channel == 2) {
            auto a = stats(actual), b = stats(expected);
            maxAoError =
                    std::max(maxAoError,
                             c["roi"].toObject()["full"].toObject()["meanAbsoluteRgb"].toDouble());
            // Far-plane background must not self-occlude through packed depth.
            if (!actual.isNull() && !expected.isNull() && expected.pixelColor(0, 0).red() == 255)
                passed = passed && actual.pixelColor(0, 0).red() == 255;
            channels.append(QJsonObject{ { "index", index },
                                         { "channel", channel },
                                         { "actualMin", a.minimum },
                                         { "actualMean", a.mean },
                                         { "actualNonWhite", a.belowWhite },
                                         { "expectedMin", b.minimum },
                                         { "expectedMean", b.mean },
                                         { "expectedNonWhite", b.belowWhite } });
            if (index >= 80)
                passed = passed && a.belowWhite > 100 && b.belowWhite > 100;
        }
        const int targetWidth = c["targetWidth"].toInt(), targetHeight = c["targetHeight"].toInt();
        passed = passed && targetWidth > 0 && targetHeight > 0
                && qint64(targetWidth) * targetHeight <= 8294400 && targetWidth <= 4096
                && targetHeight <= 4096;
    }
    const auto shadowQt = pair(QImage(args[2] + "/086.png"), QImage(args[2] + "/087.png"));
    const auto shadowWeb = pair(QImage(args[3] + "/086.png"), QImage(args[3] + "/087.png"));
    passed = passed && maxGeometryError < .05 && maxAoError < .05 && maxSingleMae <= 6
            && minSingleSsim >= .97 && shadowQt["changedPixels"].toInt() > 100
            && shadowWeb["changedPixels"].toInt() > 100;
    QSaveFile output(args[4]);
    if (!output.open(QIODevice::WriteOnly))
        return 3;
    output.write(QJsonDocument(QJsonObject{ { "passed", passed },
                                            { "cases", cases.size() },
                                            { "maxDepthNormalFullMae", maxGeometryError },
                                            { "maxAoFullMae", maxAoError },
                                            { "maxSingleCaseRoiMae", maxSingleMae },
                                            { "minSingleCaseRoiSsim", minSingleSsim },
                                            { "aoChannels", channels },
                                            { "shadowQt", shadowQt },
                                            { "shadowWeb", shadowWeb } })
                         .toJson());
    return output.commit() && passed ? 0 : 4;
}
