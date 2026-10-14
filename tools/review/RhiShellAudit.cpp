#include "RhiItem.h"
#include "RhiReviewInput.h"
#include "SceneAdapter.h"
#include "ImageComparison.h"
#include "VerificationWindow.h"
#include <QGuiApplication>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSaveFile>
#include <QPointer>
#include <QCryptographicHash>
#include <QPainter>
#include <atomic>
#include <optional>
#include <stdexcept>
#include <cstdio>
#include <cmath>
using namespace yalahoo;
using namespace yalahoo::rhi;
using namespace yalahoo::review;
namespace {
void check(bool ok, const char *reason)
{
    if (!ok)
        throw std::runtime_error(reason);
}
QJsonObject read(const QString &path)
{
    QFile f(path);
    check(f.open(QIODevice::ReadOnly), "Missing review JSON");
    return QJsonDocument::fromJson(f.readAll()).object();
}
void await(const std::function<bool()> &done, QQuickWindow &window)
{
    QElapsedTimer timeout;
    timeout.start();
    while (!done() && timeout.elapsed() < 20000) {
        window.update();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    check(done(), "RHI shell GPU timeout");
}
QPolygonF region(const Input &in, QSize size, QRectF local)
{
    QPolygonF p;
    for (auto v : { local.topLeft(), local.topRight(), local.bottomRight(), local.bottomLeft() }) {
        auto clip = in.scene.projection * in.scene.view * in.scene.caseTransform
                * QVector4D(float(v.x()), float(v.y()), .07f, 1);
        p.append({ (clip.x() / clip.w() + 1) * .5 * size.width(),
                   (1 - clip.y() / clip.w()) * .5 * size.height() });
    }
    return p;
}
QJsonObject losses(const Input &in, const QImage &actual, const QImage &expected, double &loss)
{
    struct Region
    {
        const char *name;
        QRectF local;
    };
    const Region regions[] = { { "paper", { -.7, 1, 1.75, 1.7 } },
                               { "margin", { 1.86, 1, .23, 1.7 } },
                               { "spine", { -2.17, .7, .23, 2.2 } },
                               { "rim", { -1.78, .22, 3.5, .07 } },
                               { "background", { 2.36, .7, .22, 2.2 } } };
    QJsonObject result;
    loss = 0;
    for (const auto &r : regions) {
        auto metric = compareImages(actual, expected, region(in, actual.size(), r.local), true);
        metric["edgeDistancePixels"] =
                edgeDistance(actual, expected, region(in, actual.size(), r.local));
        check(metric["valid"].toBool() && metric["pixels"].toInt() > 0, "Invalid material ROI");
        loss += metric["meanAbsoluteRgb"].toDouble() / 5;
        if (!metric["edgeDistancePixels"].isNull())
            loss += metric["edgeDistancePixels"].toDouble() / (5 * 255);
        result[r.name] = metric;
    }
    return result;
}
void strip(const QString &path, const QImage &source, const QImage &prior, const QImage &rhi,
           const QJsonObject &metric)
{
    QPolygonF polygon;
    for (auto v : metric["polygon"].toArray()) {
        const auto p = v.toArray();
        polygon.append({ p[0].toDouble(), p[1].toDouble() });
    }
    const auto rect =
            polygon.boundingRect().toAlignedRect().adjusted(-3, -3, 3, 3).intersected(rhi.rect());
    check(!rect.isEmpty(), "Empty ROI crop");
    QImage out(rect.width() * 4 * 4, rect.height() * 4 + 26, QImage::Format_RGB32);
    out.fill(Qt::white);
    QPainter painter(&out);
    const QList<QImage> images{ source, prior, rhi, absoluteDifference(rhi, source) };
    const QStringList labels{ "Frozen source", "Quick3D", "RHI D3D11", "RGB difference" };
    for (int i = 0; i < 4; ++i) {
        painter.drawImage(QRect(i * rect.width() * 4, 26, rect.width() * 4, rect.height() * 4),
                          images[i], rect);
        painter.drawText(i * rect.width() * 4 + 3, 19, labels[i]);
    }
    painter.end();
    check(out.save(path), "Cannot save ROI strip");
}
}
int main(int argc, char **argv)
{
    yalahoo::verification::prepareApplication();
#if defined(Q_OS_WIN)
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
#elif defined(Q_OS_MACOS)
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Metal);
#else
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
#endif
    QGuiApplication app(argc, argv);
    if (argc != 4)
        return 2;
    try {
        const auto fixturePath = QString::fromLocal8Bit(argv[1]),
                   output = QString::fromLocal8Bit(argv[2]),
                   oldOutput = QString::fromLocal8Bit(argv[3]);
        const auto source = read(fixturePath);
        const auto cases = source["cases"].toArray();
        check(cases.size() == 61, "Incomplete R10B fixtures");
        QDir().mkpath(output);
        QQuickWindow window;
        window.setTitle("yalahOO!!! · R10B GPU review");
        verification::prepareWindow(window, true);
        auto *item = new RhiItem(window.contentItem());
        auto received = std::make_shared<std::optional<Capture>>();
        QPointer<QQuickWindow> guard(&window);
        item->captured = [received, guard](Capture c) {
            if (guard)
                QMetaObject::invokeMethod(
                        guard,
                        [received, c = std::move(c)]() mutable {
                            *received = std::move(c);
                        },
                        Qt::QueuedConnection);
        };
        quint64 request = 0;
        auto capture = [&](Input in, QSize size) {
            in.request = ++request;
            received->reset();
            window.resize(size);
            item->setSize(size);
            item->submit(in, size);
            window.show();
            await(
                    [&] {
                        return *received && (**received).request == request;
                    },
                    window);
            const auto result = **received;
            check(window.devicePixelRatio() == 1, "R10B requires actual DPR1");
            check(result.metadata["renderThreadDistinct"].toBool(),
                  "Missing threaded RHI execution");
            check(result.metadata["msaaSamples"].toInt() == in.captureSamples,
                  "MSAA silently downgraded");
            return result;
        };
        QJsonArray results, residuals, sorting, shadows, captureIsolation, channels;
        double rhiLoss = 0, quickLoss = 0, worstProjection = 0;
        int materialCases = 0;
        QImage previous;
        std::optional<Capture> previousCapture;
        int previousSource = -1;
        for (auto value : cases) {
            const auto c = value.toObject();
            const auto size = QSize(c["width"].toInt(), c["height"].toInt());
            const auto in = fixture(c);
            auto gpu = capture(in, size);
            gpu.metadata["input"] = c;
            gpu.metadata["reviewTime"] = 0;
            auto actual = in.rawOutput ? gpu.composite : window.grabWindow();
            const auto label = c["screenshot"].toString();
            QImage expected(QString(FIXTURE_ROOT) + "/" + source["referenceMedia"].toString()
                            + label);
            QImage prior(oldOutput + "/" + label);
            check(expected.size() == actual.size() && prior.size() == actual.size(),
                  "Mismatched source/Quick3D/RHI inputs");
            double loss = 0, priorLoss = 0;
            auto roi = losses(in, actual, expected, loss);
            auto priorRoi = losses(in, prior, expected, priorLoss);
            const int sourceIndex = c["sourceIndex"].toInt();
            if (!c["shadowProbe"].toBool()) {
                rhiLoss += loss;
                quickLoss += priorLoss;
                ++materialCases;
            }
            for (const auto &p : c["projections"].toArray()) {
                const auto witness = p.toObject();
                const auto pixel = witness["pixel"].toArray();
                const auto clip = in.scene.projection * in.scene.view
                        * QVector4D(vector(witness["world"].toArray()), 1);
                const double x = (clip.x() / clip.w() + 1) * .5 * size.width(),
                             y = (1 - clip.y() / clip.w()) * .5 * size.height();
                worstProjection =
                        std::max(worstProjection,
                                 std::hypot(x - pixel[0].toDouble(), y - pixel[1].toDouble()));
            }
            for (auto name : { "paper", "margin", "spine", "rim", "background" }) {
                const auto m = roi[name].toObject();
                if (m["meanAbsoluteRgb"].toDouble() > 6
                    || (m["ssimWindows"].toInt() > 0 && m["ssim"].toDouble() < .97))
                    residuals.append(QJsonObject{ { "sourceIndex", sourceIndex },
                                                  { "region", name },
                                                  { "metric", m },
                                                  { "quick3d", priorRoi[name] } });
                if (sourceIndex == 2 || sourceIndex == 7 || sourceIndex == 22 || sourceIndex == 25
                    || sourceIndex == 28 || sourceIndex == 37 || sourceIndex == 61
                    || sourceIndex == 62)
                    strip(output + QString("/roi-%1-%2.png").arg(sourceIndex).arg(name), expected,
                          prior, actual, m);
            }
            check(actual.save(output + "/" + label), "Cannot save RHI image");
            absoluteDifference(actual, expected).save(output + "/diff-" + label);
            gpu.color.save(output + "/capture-" + label);
            gpu.compositeNormal.save(output + "/normal-" + label);
            gpu.compositeDepth.save(output + "/depth-" + label);
            QJsonObject result{ { "sourceIndex", sourceIndex },
                                { "roi", roi },
                                { "quick3dRoi", priorRoi },
                                { "equalWeightLoss", loss },
                                { "quick3dEqualWeightLoss", priorLoss },
                                { "metadata", gpu.metadata } };
            if (sourceIndex >= 9 && sourceIndex <= 20 && sourceIndex % 2 == 0) {
                check(previousSource == sourceIndex - 1 && previousCapture,
                      "Missing shadow off witness");
                int changed = 0;
                for (int y = 0; y < size.height(); ++y)
                    for (int x = 0; x < size.width(); ++x) {
                        // Difference only where shell depth differs from the opaque receiver.
                        if (gpu.compositeDepth.pixel(x, y) != gpu.depth.pixel(x, y)
                            && std::abs(actual.pixelColor(x, y).red()
                                        - previous.pixelColor(x, y).red())
                                    > 1)
                            ++changed;
                    }
                check(changed > 10, "Shell shadow reception did not change real pixels");
                shadows.append(QJsonObject{ { "sourceIndex", sourceIndex },
                                            { "shellChangedPixels", changed },
                                            { "shadowSize", in.shadowSize } });
            }
            if (sourceIndex == 22 || sourceIndex == 25 || sourceIndex >= 62) {
                auto reverse = in;
                std::reverse(reverse.entities.begin(), reverse.entities.end());
                std::reverse(reverse.entityIds.begin(), reverse.entityIds.end());
                const auto reversed = capture(reverse, size);
                check(reversed.composite == gpu.composite
                              && reversed.compositeDepth == gpu.compositeDepth,
                      "Shell output depends on instance submission order");
                check(reversed.metadata["sortedShells"] == gpu.metadata["sortedShells"],
                      "Unstable global shell ordering");
                sorting.append(QJsonObject{ { "sourceIndex", sourceIndex },
                                            { "reversePixelsIdentical", true } });
            }
            if (sourceIndex == 21 || sourceIndex == 25) {
                auto opaqueOnly = in;
                opaqueOnly.shells = false;
                const auto isolated = capture(opaqueOnly, size);
                check(isolated.color == gpu.color && isolated.normal == gpu.normal
                              && isolated.depth == gpu.depth,
                      "Shells leaked into opaque transmission capture");
                captureIsolation.append(QJsonObject{ { "sourceIndex", sourceIndex },
                                                     { "opaqueCaptureIdentical", true } });
            }
            results.append(result);
            previous = actual;
            previousCapture = gpu;
            previousSource = sourceIndex;
            std::printf("R10B %d source=%d loss=%.4f Quick3D=%.4f\n", c["index"].toInt(),
                        sourceIndex, loss, priorLoss);
            std::fflush(stdout);
        }
        const auto original =
                read(QString(FIXTURE_ROOT) + "/tests/fixtures/render-backend/visual.json")["cases"]
                        .toArray();
        for (int index : { 58, 59 }) {
            auto in = fixture(original[index].toObject());
            in.geometryDiagnostic = true;
            in.shells = false;
            in.captureSamples = 1;
            const auto gpu = capture(in, { 960, 720 });
            auto actual = gpu.normal;
            if (index == 58) {
                actual = QImage(gpu.depth.size(), QImage::Format_RGB32);
                for (int y = 0; y < actual.height(); ++y)
                    for (int x = 0; x < actual.width(); ++x) {
                        const auto p = gpu.depth.pixelColor(x, y);
                        const double linear =
                                (p.red() * 65536 + p.green() * 256 + p.blue()) / 16777215.0;
                        const double z = in.scene.nearPlane
                                + linear * (in.scene.farPlane - in.scene.nearPlane);
                        const double depth =
                                in.scene.farPlane / (in.scene.farPlane - in.scene.nearPlane)
                                - in.scene.farPlane * in.scene.nearPlane
                                        / ((in.scene.farPlane - in.scene.nearPlane) * z);
                        const int channel = std::clamp(int(std::lround(depth * 255)), 0, 255);
                        actual.setPixelColor(x, y, QColor(channel, channel, channel));
                    }
            }
            const QImage expected(QString(FIXTURE_ROOT)
                                  + QString("/docs/media/render-backend/web/%1.png")
                                            .arg(index, 3, 10, QChar('0')));
            const QPolygonF all{ { 0, 0 }, { 960, 0 }, { 960, 720 }, { 0, 720 } };
            const auto metric = compareImages(actual, expected, all, false);
            check(metric["valid"].toBool() && metric["meanAbsoluteRgb"].toDouble() < .05,
                  "Source neighbor depth/normal channel mismatch");
            actual.save(output + QString("/channel-%1-rhi.png").arg(index));
            channels.append(QJsonObject{ { "sourceIndex", index }, { "metrics", metric } });
        }
        // Equal-depth ties use stable IDs, even when draw order is reversed.
        auto tied = fixture(cases[18].toObject());
        tied.entities = { QMatrix4x4(), QMatrix4x4() };
        tied.entities[0].translate(-.3f, 0, 0);
        tied.entities[1].translate(.3f, 0, 0);
        tied.entityIds = { 20, 10 };
        const auto tie = capture(tied, { 960, 720 });
        std::reverse(tied.entities.begin(), tied.entities.end());
        std::reverse(tied.entityIds.begin(), tied.entityIds.end());
        const auto tieReverse = capture(tied, { 960, 720 });
        check(tie.composite == tieReverse.composite
                      && tie.metadata["sortedShells"] == tieReverse.metadata["sortedShells"],
              "Equal-depth identity tie is unstable");
        auto duplicate = tied;
        duplicate.entityIds = { 10, 10 };
        bool rejected = false;
        try {
            item->submit(duplicate, { 960, 720 });
        } catch (const std::exception &) {
            rejected = true;
        }
        check(rejected, "Duplicate stable identity was accepted");
        // Synthetic alpha/contain boundary: MSAA changes edge coverage while preserving print centers.
        auto boundary = fixture(cases[18].toObject());
        boundary.shells = false;
        boundary.entities.clear();
        boundary.entityIds.clear();
        boundary.lightingEnabled = false;
        boundary.cover = QImage(64, 64, QImage::Format_RGBA8888);
        boundary.cover.fill(QColor(210, 40, 80));
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
                boundary.cover.setPixelColor(x, y, Qt::transparent);
        boundary.captureSamples = 1;
        const auto single = capture(boundary, { 960, 720 });
        boundary.captureSamples = 4;
        const auto multi = capture(boundary, { 960, 720 });
        int changedEdges = 0;
        int darkFringePixels = 0;
        const int red = multi.color.pixelColor(480, 360).red();
        for (int y = 0; y < 720; ++y)
            for (int x = 0; x < 960; ++x) {
                changedEdges += single.color.pixel(x, y) != multi.color.pixel(x, y);
                darkFringePixels += multi.color.pixelColor(x, y).red() < red - 1;
            }
        check(changedEdges > 100 && single.color.pixel(480, 360) == multi.color.pixel(480, 360),
              "MSAA alpha resolve failed or blurred print center");
        check(darkFringePixels == 0, "Transparent black texels created a dark print fringe");
        check(multi.normal.pixelColor(300, 180) == multi.normal.pixelColor(0, 0),
              "Transparent print margin wrote normal/depth");
        single.color.save(output + "/alpha-1x.png");
        multi.color.save(output + "/alpha-4x.png");
        check(worstProjection <= 2 && shadows.size() == 6 && sorting.size() == 6,
              "Missing structural/shadow/sort acceptance");
        QJsonObject report{ { "round", "R10B" },
                            { "windowPolicy", verification::windowPolicy(window) },
                            { "configuration", "Release" },
                            { "qt", qVersion() },
                            { "baseline", source["baseline"] },
                            { "cases", results },
                            { "residuals", residuals },
                            { "fixtureSha256",
                              QString::fromLatin1(QCryptographicHash::hash(
                                                          [&] {
                                                              QFile f(fixturePath);
                                                              check(f.open(QIODevice::ReadOnly),
                                                                    "Missing fixture hash input");
                                                              return f.readAll();
                                                          }(),
                                                          QCryptographicHash::Sha256)
                                                          .toHex()) },
                            { "dpr", window.devicePixelRatio() },
                            { "meanEqualWeightLoss", rhiLoss / materialCases },
                            { "quick3dMeanEqualWeightLoss", quickLoss / materialCases },
                            { "maxProjectionPixels", worstProjection },
                            { "shadowPairs", shadows },
                            { "sorting", sorting },
                            { "captureIsolation", captureIsolation },
                            { "neighborChannels", channels },
                            { "equalDepthTie", true },
                            { "duplicateIdRejected", rejected },
                            { "alphaResolve",
                              QJsonObject{ { "changedEdgePixels", changedEdges },
                                           { "centerUnchanged", true },
                                           { "darkFringePixels", darkFringePixels },
                                           { "samples", 4 } } },
                            { "productImplementationReady", false },
                            { "passed", rhiLoss < quickLoss } };
        QSaveFile f(output + "/runtime.json");
        check(verification::windowPolicySatisfied(window),
              "GPU review window violated bottom/no-focus policy");
        check(f.open(QIODevice::WriteOnly), "Cannot save GPU report");
        f.write(QJsonDocument(report).toJson());
        check(f.commit(), "Cannot commit GPU report");
        check(rhiLoss < quickLoss, "Material residual did not shrink");
        item->captured = { };
        window.hide();
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "R10B: %s\n", e.what());
        return 1;
    }
}
