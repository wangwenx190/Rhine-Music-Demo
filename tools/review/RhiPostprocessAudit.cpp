#include "RhiReviewInput.h"
#include "RenderQuality.h"
#include "ImageComparison.h"
#include "VerificationWindow.h"
#include <QGuiApplication>
#include <QQuickWindow>
#include <QQuickGraphicsConfiguration>
#include <QSGRendererInterface>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QPointer>
#include <QSaveFile>
#include <QDir>
#include <QPainter>
#include <QTimer>
#include <optional>
#include <mutex>
#include <chrono>
#include <cstdio>
#include <stdexcept>
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
    check(f.open(QIODevice::ReadOnly), "Missing R10C fixture");
    return QJsonDocument::fromJson(f.readAll()).object();
}
void wait(const std::function<bool()> &done, QQuickWindow &window)
{
    QElapsedTimer timeout;
    timeout.start();
    while (!done() && timeout.elapsed() < 30000) {
        window.update();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    check(done(), "R10C GPU timeout");
}
QJsonObject losses(const Input &in, const QImage &actual, const QImage &source)
{
    QJsonObject result;
    const std::array<std::pair<const char *, QRectF>, 5> regions{
        std::pair{ "paper", QRectF(-.7, 1, 1.75, 1.7) },
        std::pair{ "margin", QRectF(1.86, 1, .23, 1.7) },
        std::pair{ "spine", QRectF(-2.17, .7, .23, 2.2) },
        std::pair{ "rim", QRectF(-1.78, .22, 3.5, .07) },
        std::pair{ "background", QRectF(2.36, .7, .22, 2.2) }
    };
    for (const auto &[name, local] : regions) {
        QPolygonF polygon;
        for (const auto &v :
             { local.topLeft(), local.topRight(), local.bottomRight(), local.bottomLeft() }) {
            auto clip = in.scene.projection * in.scene.view * in.scene.caseTransform
                    * QVector4D(float(v.x()), float(v.y()), .07f, 1);
            polygon.append({ (clip.x() / clip.w() + 1) * .5 * actual.width(),
                             (1 - clip.y() / clip.w()) * .5 * actual.height() });
        }
        auto metric = compareImages(actual, source, polygon, true);
        metric["edgeDistancePixels"] = edgeDistance(actual, source, polygon);
        check(metric["valid"].toBool(), "Invalid final-stage ROI");
        result[name] = metric;
    }
    return result;
}
double equalWeightLoss(const QJsonObject &regions)
{
    double loss = 0;
    for (const auto &value : regions) {
        const auto metric = value.toObject();
        loss += metric["meanAbsoluteRgb"].toDouble() / 5;
        if (!metric["edgeDistancePixels"].isNull())
            loss += metric["edgeDistancePixels"].toDouble() / (5 * 255);
    }
    return loss;
}
qint64 changed(const QImage &a, const QImage &b)
{
    check(a.size() == b.size(), "Mismatched GPU images");
    qint64 count = 0;
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x)
            count += a.pixel(x, y) != b.pixel(x, y);
    return count;
}
}
int main(int argc, char **argv)
{
#if defined(Q_OS_WIN)
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
#elif defined(Q_OS_MACOS)
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Metal);
#else
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
#endif
    QGuiApplication app(argc, argv);
    if (argc == 2 && QByteArray(argv[1]) == "--adapters") {
        std::printf("%s\n", QJsonDocument(adapterInventory()).toJson().constData());
        return 0;
    }
    const bool shortProfile = argc == 3 && QByteArray(argv[2]) == "--profile";
    const bool recordPerformance = argc == 3 && QByteArray(argv[2]) == "--record-performance";
    const bool fullReplay = argc == 2 || recordPerformance;
    if (argc != 2
        && (argc != 3
            || (!shortProfile && !recordPerformance && QByteArray(argv[2]) != "--diagnostics")))
        return 2;
    try {
        const QString root = QString::fromUtf8(FIXTURE_ROOT),
                      output = QString::fromLocal8Bit(argv[1]);
        QDir().mkpath(output);
        QQuickWindow window;
        if (shortProfile) {
            auto configuration = window.graphicsConfiguration();
            configuration.setTimestamps(true);
            window.setGraphicsConfiguration(configuration);
        }
        window.setTitle("yalahOO!!! · R10C GPU review");
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
        auto capture = [&](Input in, QSize size, QSize target) {
            in.request = ++request;
            received->reset();
            window.resize(size);
            item->setSize(size);
            item->submit(in, target);
            window.show();
            wait(
                    [&] {
                        return *received && (**received).request == request;
                    },
                    window);
            check(verification::windowPolicySatisfied(window), "R10C window became active/topmost");
            return **received;
        };
        const auto original = read(root + "/tests/fixtures/render-backend/visual.json");
        QJsonArray results, residuals;
        double totalLoss = 0, quickLoss = 0;
        const auto cases = original["cases"].toArray();
        for (int i = 0; fullReplay && i < 66; ++i) {
            const auto c = cases[i].toObject();
            if (c["channel"].toInt() != 0)
                continue;
            auto in = fixture(c);
            in.rawOutput = false;
            in.postprocess = !c["materialBaseline"].toBool();
            const auto q = RenderQuality::authoredPreset(c["quality"].toString());
            in.aoSamples = q["aoSamples"].toInt();
            in.aoResolution = q["aoResolution"].toFloat();
            in.bokeh = q["depthOfField"].toInt() > 0 && !c["disableBokeh"].toBool();
            in.smaa = q["antialias"].toString() == "smaa" && !c["disableSmaa"].toBool();
            in.focus = float(c["focus"].toDouble());
            in.aperture = float(c["aperture"].toDouble());
            const QSize size(c["width"].toInt(), c["height"].toInt());
            const QSize target(c["targetWidth"].toInt(), c["targetHeight"].toInt());
            const auto gpu = capture(in, size, target);
            const auto actual = window.grabWindow();
            QImage source(root + "/docs/media/render-backend/web/" + c["screenshot"].toString());
            check(actual.size() == source.size(), "Final source size mismatch");
            const auto roi = losses(in, actual, source);
            QImage prior(root + "/docs/media/render-backend/qt/" + c["screenshot"].toString());
            check(prior.size() == source.size(), "Final Quick3D reference size mismatch");
            const auto priorRoi = losses(in, prior, source);
            totalLoss += equalWeightLoss(roi);
            quickLoss += equalWeightLoss(priorRoi);
            for (const auto &name : roi.keys()) {
                const auto metric = roi[name].toObject();
                if (metric["meanAbsoluteRgb"].toDouble() > 6
                    || (metric["ssimWindows"].toInt() > 0 && metric["ssim"].toDouble() < .97))
                    residuals.append(QJsonObject{ { "sourceIndex", i },
                                                  { "roi", name },
                                                  { "metric", metric },
                                                  { "quick3d", priorRoi[name] } });
            }
            actual.save(output + "/" + c["screenshot"].toString());
            absoluteDifference(actual, source).save(output + "/diff-" + c["screenshot"].toString());
            results.append(QJsonObject{ { "sourceIndex", i },
                                        { "roi", roi },
                                        { "quick3dRoi", priorRoi },
                                        { "equalWeightLoss", equalWeightLoss(roi) },
                                        { "quick3dEqualWeightLoss", equalWeightLoss(priorRoi) },
                                        { "metadata", gpu.metadata },
                                        { "targetWidth", target.width() },
                                        { "targetHeight", target.height() } });
            std::printf("R10C final source=%d\n", i);
            std::fflush(stdout);
        }
        check(results.isEmpty() || totalLoss < quickLoss,
              "Final material residual did not shrink relative to the same-source Quick3D replay");
        auto test = fixture(cases[3].toObject());
        test.rawOutput = true;
        test.postprocess = true;
        test.focus = float(cases[3].toObject()["focus"].toDouble());
        test.aperture = .0003f;
        const auto plain = capture(test, { 960, 720 }, { 960, 720 });
        test.bokeh = true;
        const auto bokeh = capture(test, { 960, 720 }, { 960, 720 });
        check(changed(plain.postprocessed, bokeh.postprocessed) > 100,
              "Bokeh did not change pixels");
        test.bokeh = false;
        test.smaa = true;
        const auto smaa = capture(test, { 960, 720 }, { 960, 720 });
        check(changed(plain.postprocessed, smaa.postprocessed) > 10, "SMAA did not change pixels");
        test = fixture(read(root + "/tests/fixtures/occlusion/visual.json")["cases"]
                               .toArray()[80]
                               .toObject());
        test.rawOutput = true;
        test.postprocess = true;
        test.smaa = false;
        test.aoSamples = 32;
        test.aoRadius = .38f;
        // Explicit algorithm stress probe, matching R09 near/far diagnostics.
        test.scene.nearPlane = .1;
        test.scene.farPlane = 100;
        test.scene.projection.setToIdentity();
        test.scene.projection.perspective(32, 4.0f / 3, .1f, 100);
        const auto ao = capture(test, { 960, 720 }, { 960, 720 });
        test.aoSamples = 0;
        const auto noAo = capture(test, { 960, 720 }, { 960, 720 });
        check(changed(ao.postprocessed, noAo.postprocessed) > 10,
              "AO did not change stress pixels");
        const QPolygonF full{ { 0, 0 }, { 960, 0 }, { 960, 720 }, { 0, 720 } };
        QJsonArray aoOracle;
        for (const auto &entry : { std::pair{ 81, ao.rawAo }, std::pair{ 82, ao.blurredAo },
                                   std::pair{ 84, ao.aoNormal } }) {
            const QImage source(root
                                + QString("/docs/media/occlusion/web/%1.png")
                                          .arg(entry.first, 3, 10, QChar('0')));
            const auto metric = compareImages(entry.second, source, full, false);
            entry.second.save(output + QString("/ao-oracle-%1.png").arg(entry.first));
            std::printf("AO oracle %d MAE=%.8f\n", entry.first,
                        metric["meanAbsoluteRgb"].toDouble());
            std::fflush(stdout);
            check(metric["valid"].toBool() && metric["meanAbsoluteRgb"].toDouble() < .05,
                  "RHI raw AO/blur/normal source mismatch");
            aoOracle.append(QJsonObject{ { "sourceIndex", entry.first }, { "metric", metric } });
            entry.second.save(output + QString("/ao-oracle-%1.png").arg(entry.first));
        }
        QJsonObject effects{ { "bokehPixels", changed(plain.postprocessed, bokeh.postprocessed) },
                             { "smaaPixels", changed(plain.postprocessed, smaa.postprocessed) },
                             { "aoStressPixels", changed(ao.postprocessed, noAo.postprocessed) } };
        auto coverInput = fixture(cases[25].toObject());
        const auto firstCover = capture(coverInput, { 960, 720 }, { 960, 720 });
        const auto replayCover = capture(coverInput, { 960, 720 }, { 960, 720 });
        check(firstCover.metadata["coverSnapshotCopies"]
                              == replayCover.metadata["coverSnapshotCopies"]
                      && firstCover.atlas == replayCover.atlas,
              "Unchanged cover copied or changed on synchronization");
        coverInput.cover.fill(Qt::blue);
        const auto changedCover = capture(coverInput, { 960, 720 }, { 960, 720 });
        check(changedCover.metadata["coverSnapshotCopies"].toInteger()
                              == replayCover.metadata["coverSnapshotCopies"].toInteger() + 1
                      && changedCover.atlas.pixelColor(0, 0) == QColor(Qt::blue),
              "New cover revision was not copied/uploaded");
        auto atlas = fixture(cases[25].toObject());
        atlas.postprocess = true;
        atlas.aoSamples = 32;
        atlas.bokeh = true;
        atlas.focus = float(cases[25].toObject()["focus"].toDouble());
        atlas.atlasGeneration = 1;
        atlas.atlasSize = { 2048, 2048 };
        atlas.initializingAtlas = true;
        atlas.entities.clear();
        atlas.entityIds.clear();
        QImage page(atlas.atlasSize, QImage::Format_RGBA8888);
        for (int slot = 0; slot < 64; ++slot) {
            QImage pixels(256, 256, QImage::Format_RGBA8888);
            pixels.fill(QColor(40 + slot * 3, 180 - slot * 2, 70 + slot * 2));
            atlas.atlasUploads.append({ 1, 1, 1, slot, pixels });
            QPainter painter(&page);
            painter.drawImage((slot % 8) * 256, (slot / 8) * 256, pixels);
        }
        for (int lane = -4; lane <= 4; ++lane)
            for (int row = -24; row < 24; ++row) {
                const int index = atlas.entities.size(), slot = (index + 40) % 64;
                QMatrix4x4 model;
                model.translate(lane * 5.2f, 0, row * .62f);
                atlas.entities.append(model);
                atlas.entityIds.append(quint64(index + 1));
                atlas.coverTiles.append({ (slot % 8) * 256, (slot / 8) * 256, 256, 256 });
            }
        const auto initial = capture(atlas, { 960, 720 }, { 960, 720 });
        initial.composite.save(output + "/atlas-432-composite.png");
        check(initial.atlas == page, "Initialized atlas GPU bytes differ");
        auto native = initial.metadata["nativeAtlasTransfers"].toObject();
        const bool nativeProof = native["available"].toBool();
#if defined(Q_OS_WIN)
        check(nativeProof, "Windows D3D11 native transfer proof unavailable");
#endif
        check((nativeProof ? native["destinationBytes"].toInteger()
                           : initial.metadata["atlasUploadBytes"].toInteger())
                      == 16777216,
              "Initialization native transfer bytes unavailable/over budget");
        atlas.initializingAtlas = false;
        atlas.atlasUploads.clear();
        const auto stable = capture(atlas, { 960, 720 }, { 960, 720 });
        check(stable.metadata["atlasUploadBytes"].toInteger() == 0
                      && (!nativeProof
                          || stable.metadata["nativeAtlasTransfers"]
                                          .toObject()["destinationBytes"]
                                          .toInteger()
                                  == 0)
                      && stable.atlas == initial.atlas
                      && stable.postprocessed == initial.postprocessed,
              "Stable atlas frame uploaded or changed");
        QJsonArray capacityCases;
        for (int count : { 1, 7, 8, 31, 32, 33, 64, 432 }) {
            auto bounded = atlas;
            bounded.entities.resize(count);
            bounded.entityIds.resize(count);
            bounded.coverTiles.resize(count);
            const auto result = capture(bounded, { 960, 720 }, { 960, 720 });
            check(result.metadata["generation"] == stable.metadata["generation"],
                  "Entity count/visibility recreated the render graph");
            check(result.metadata["uniformUploadBytes"].toInteger()
                          <= result.metadata["uniformCapacityBytes"].toInteger(),
                  "Uniform prefix exceeds its fixed capacity");
            check(result.atlas == stable.atlas, "Entity count changed atlas pixels");
            capacityCases.append(
                    QJsonObject{ { "instances", count },
                                 { "generation", result.metadata["generation"] },
                                 { "uploadBytes", result.metadata["uniformUploadBytes"] },
                                 { "capacityBytes", result.metadata["uniformCapacityBytes"] } });
        }
        auto fullVisibility = atlas;
        fullVisibility.scene.view.setToIdentity();
        fullVisibility.scene.view.lookAt({ 0, 2, 100 }, { 0, 2, 0 }, { 0, 1, 0 });
        fullVisibility.scene.projection.setToIdentity();
        fullVisibility.scene.projection.ortho(-30, 30, -30, 30, .1f, 300);
        const auto maximum = capture(fullVisibility, { 960, 720 }, { 960, 720 });
        check(maximum.metadata["visibleEntities"].toInt() == 432
                      && maximum.metadata["uniformUploadBytes"].toInteger()
                              <= maximum.metadata["uniformCapacityBytes"].toInteger()
                      && maximum.metadata["generation"] == stable.metadata["generation"],
              "Fully visible pool exceeded capacity or rebuilt the render graph");
        capacityCases.append(
                QJsonObject{ { "instances", 432 },
                             { "allVisible", true },
                             { "generation", maximum.metadata["generation"] },
                             { "uploadBytes", maximum.metadata["uniformUploadBytes"] },
                             { "capacityBytes", maximum.metadata["uniformCapacityBytes"] } });
        QImage replacement(256, 256, QImage::Format_RGBA8888);
        replacement.fill(QColor(230, 30, 90));
        atlas.atlasUploads = { { 1, 2, 2, 0, replacement } };
        const auto tile = capture(atlas, { 960, 720 }, { 960, 720 });
        native = tile.metadata["nativeAtlasTransfers"].toObject();
        check((nativeProof ? native["destinationBytes"].toInteger()
                           : tile.metadata["atlasUploadBytes"].toInteger())
                              == 262144
                      && (!nativeProof || native["regions"].toArray().size() == 1),
              "One tile caused a native whole-page transfer");
        QImage expected = page.copy();
        {
            QPainter painter(&expected);
            painter.drawImage(0, 0, replacement);
        }
        check(tile.atlas == expected && changed(tile.postprocessed, initial.postprocessed) > 0,
              "Atlas tile changed wrong GPU region or did not reach instances");
        atlas.atlasUploads[0].serial = 3;
        atlas.atlasUploads[0].pixels = page.copy(0, 0, 256, 256);
        const auto revert = capture(atlas, { 960, 720 }, { 960, 720 });
        check(revert.atlas == initial.atlas && revert.postprocessed == initial.postprocessed,
              "Atlas revert changed pixels");
        atlas.atlasUploads.clear();
        std::reverse(atlas.entities.begin(), atlas.entities.end());
        std::reverse(atlas.entityIds.begin(), atlas.entityIds.end());
        std::reverse(atlas.coverTiles.begin(), atlas.coverTiles.end());
        const auto reverse = capture(atlas, { 960, 720 }, { 960, 720 });
        check(reverse.postprocessed == initial.postprocessed
                      && reverse.metadata["sortedShells"] == initial.metadata["sortedShells"],
              "432-instance order/atlas assignment changed");
        // Target rebuild must retain atlas resource and must not upload it again.
        const auto resized = capture(atlas, { 1280, 720 }, { 1280, 720 });
        check(resized.atlas == page && resized.metadata["atlasUploadBytes"].toInteger() == 0
                      && resized.metadata["nativeAtlasTransfers"]
                                      .toObject()["destinationBytes"]
                                      .toInteger()
                              == 0,
              "Resize reuploaded/lost atlas");
        atlas.atlasGeneration = 2;
        atlas.initializingAtlas = true;
        for (int slot = 0; slot < 64; ++slot)
            atlas.atlasUploads.append(
                    { 2, 4, 1, slot, page.copy((slot % 8) * 256, (slot / 8) * 256, 256, 256) });
        atlas.atlasUploads.append({ 1, 99, 100, 0, replacement });
        const auto rebuilt = capture(atlas, { 960, 720 }, { 960, 720 });
        check(rebuilt.atlas == page && rebuilt.metadata["atlasRejected"].toInteger() > 0,
              "Destroyed atlas accepted old generation");
        atlas.initializingAtlas = false;
        atlas.atlasUploads.clear();
        for (int slot = 0; slot < 9; ++slot)
            atlas.atlasUploads.append({ 2, 5, 2, slot, replacement });
        const auto budget = capture(atlas, { 960, 720 }, { 960, 720 });
        check((nativeProof ? budget.metadata["nativeAtlasTransfers"]
                                     .toObject()["destinationBytes"]
                                     .toInteger()
                           : budget.metadata["atlasUploadBytes"].toInteger())
                              == 2097152
                      && budget.metadata["atlasPending"].toInt() == 1,
              "Steady queue did not defer the ninth tile");
        atlas.atlasUploads.clear();
        const auto drained = capture(atlas, { 960, 720 }, { 960, 720 });
        check(drained.metadata["atlasPending"].toInt() == 0, "Deferred atlas queue did not drain");
        initial.postprocessed.save(output + "/atlas-432-initial.png");
        tile.postprocessed.save(output + "/atlas-432-tile.png");
        QJsonObject atlasReport{ { "instances", 432 },           { "initial", initial.metadata },
                                 { "stable", stable.metadata },  { "tile", tile.metadata },
                                 { "revert", revert.metadata },  { "reverse", reverse.metadata },
                                 { "resize", resized.metadata }, { "rebuild", rebuilt.metadata },
                                 { "gpuPageExact", true },       { "unchangedOutsideTile", true },
                                 { "stablePixelsExact", true },  { "revertPixelsExact", true } };
        QJsonObject report{ { "round", "R10C" },
                            { "qt", qVersion() },
                            { "baseline", original["baseline"] },
                            { "cases", results },
                            { "residuals", residuals },
                            { "effects", effects },
                            { "atlas", atlasReport },
                            { "windowPolicy", verification::windowPolicy(window) },
                            { "configuration", "Release" },
                            { "productImplementationReady", false },
                            { "passed", true } };
        report["steadyBudget"] = QJsonObject{ { "firstFrame", budget.metadata },
                                              { "drainedFrame", drained.metadata } };
        report["nativeTransferByteProof"] = nativeProof;
        report["coverSnapshotReuse"] = true;
        report["capacityCases"] = capacityCases;
        if (!results.isEmpty()) {
            report["meanEqualWeightLoss"] = totalLoss / results.size();
            report["quick3dMeanEqualWeightLoss"] = quickLoss / results.size();
        }
        if (fullReplay || shortProfile) {
            // Actual presentation cadence, independent from readback and CPU submission timing.
            struct Samples
            {
                std::mutex mutex;
                std::chrono::steady_clock::time_point last;
                QVector<double> intervals, cpu, gpu;
                std::array<QVector<double>, 6> stageGpu;
                std::array<QVector<double>, 6> stageCadence;
                int stage = 0;
                bool active = false;
                qint64 uploadBytes = 0;
                qint64 uniformBytes = 0;
                qint64 generation = -1;
                int resourceChanges = 0;
            };
            auto samples = std::make_shared<Samples>();
            const auto connection = QObject::connect(
                    &window, &QQuickWindow::frameSwapped, &window,
                    [samples] {
                        std::scoped_lock lock(samples->mutex);
                        const auto now = std::chrono::steady_clock::now();
                        if (samples->active && samples->last.time_since_epoch().count()) {
                            const double interval =
                                    std::chrono::duration<double, std::milli>(now - samples->last)
                                            .count();
                            samples->intervals.append(interval);
                            samples->stageCadence[samples->stage].append(interval);
                        }
                        samples->last = now;
                    },
                    Qt::DirectConnection);
            item->profiled = [samples](QJsonObject sample) {
                std::scoped_lock lock(samples->mutex);
                if (!samples->active)
                    return;
                samples->cpu.append(sample["cpuSubmitMs"].toDouble());
                if (!sample["gpuFrameMs"].isNull())
                    samples->gpu.append(sample["gpuFrameMs"].toDouble());
                if (!sample["gpuFrameMs"].isNull())
                    samples->stageGpu[samples->stage].append(sample["gpuFrameMs"].toDouble());
                samples->uploadBytes += sample["atlasUploadBytes"].toInteger();
                samples->uniformBytes += sample["uniformUploadBytes"].toInteger();
                const auto generation = sample["resourceGeneration"].toInteger();
                if (samples->generation >= 0 && samples->generation != generation)
                    ++samples->resourceChanges;
                samples->generation = generation;
            };
            item->captured = { };
            atlas.profile = true;
            atlas.shadows = true;
            atlas.rawOutput = false;
            SceneAdapter production;
            production.setReducedMotion(true);
            production.layout()->setViewport(1920, 1080);
            atlas.scene = production.frame();
            const auto pose = atlas.scene.camera;
            for (auto &entity : atlas.entities)
                entity.translate(0, float(atlas.scene.casePosition.y),
                                 float(atlas.scene.casePosition.z));
            window.resize(1920, 1080);
            item->setSize({ 1920, 1080 });
            QElapsedTimer elapsed;
            elapsed.start();
            const int warmup = shortProfile ? 2000 : 5000, duration = shortProfile ? 30000 : 120000;
            QEventLoop presentationLoop;
            QObject driveContext;
            QTimer stopTimer;
            stopTimer.setSingleShot(true);
            stopTimer.setTimerType(Qt::PreciseTimer);
            QObject::connect(&stopTimer, &QTimer::timeout, &presentationLoop, &QEventLoop::quit);
            const auto advanceFrame = [&] {
                if (elapsed.elapsed() >= warmup + duration)
                    return;
                const auto seconds = elapsed.elapsed() / 1000.0;
                const int stage = shortProfile
                        ? std::clamp(int((elapsed.elapsed() - warmup) / 5000), 0, 5)
                        : 0;
                atlas.shadows = stage != 1 && stage != 5;
                atlas.aoSamples = stage == 2 || stage == 5 ? 0 : 32;
                atlas.bokeh = stage != 3 && stage != 5;
                atlas.shells = stage != 4 && stage != 5;
                atlas.postprocess = stage != 5;
                const float delta = float(.15 * std::sin(seconds * .2));
                const QVector3D position(float(pose.position.x) + delta, float(pose.position.y),
                                         float(pose.position.z) - delta);
                atlas.scene.camera.position = { position.x(), position.y(), position.z() };
                atlas.scene.view.setToIdentity();
                atlas.scene.view.lookAt(position, pose.aim.qt(), { 0, 1, 0 });
                atlas.focus = -(atlas.scene.view
                                * QVector4D(0, float(atlas.scene.casePosition.y) + 2,
                                            float(atlas.scene.casePosition.z), 1))
                                       .z();
                item->submit(atlas, { 1920, 1080 });
                {
                    std::scoped_lock lock(samples->mutex);
                    samples->stage = stage;
                    samples->active = elapsed.elapsed() >= warmup
                            && (!shortProfile || (elapsed.elapsed() - warmup) % 5000 >= 1000);
                }
                window.update();
            };
            // Submit once per completed presentation, through the normal GUI event loop.
            // A polling/sleep driver adds unrelated GUI work and wake-up jitter to the gate.
            const auto drivingConnection =
                    QObject::connect(&window, &QQuickWindow::frameSwapped, &driveContext,
                                     advanceFrame, Qt::QueuedConnection);
            advanceFrame();
            stopTimer.start(warmup + duration);
            presentationLoop.exec();
            QObject::disconnect(drivingConnection);
            {
                std::scoped_lock lock(samples->mutex);
                samples->active = false;
            }
            QObject::disconnect(connection);
            auto distribution = [](QVector<double> values) {
                QJsonObject result{ { "count", values.size() } };
                if (values.isEmpty())
                    return result;
                int consecutive = 0, longest = 0;
                for (double interval : values) {
                    consecutive = interval > 20 ? consecutive + 1 : 0;
                    longest = std::max(longest, consecutive);
                }
                result["maxConsecutiveOver20Ms"] = longest;
                result["overTwo60HzPeriods"] =
                        int(std::count_if(values.begin(), values.end(), [](double v) {
                            return v > 1000.0 / 30;
                        }));
                std::sort(values.begin(), values.end());
                result["p50Ms"] = values[int((values.size() - 1) * .5)];
                result["p95Ms"] = values[int((values.size() - 1) * .95)];
                result["p99Ms"] = values[int((values.size() - 1) * .99)];
                result["maxMs"] = values.last();
                result["over50Ms"] = int(std::count_if(values.begin(), values.end(), [](double v) {
                    return v > 50;
                }));
                return result;
            };
            QJsonObject performance;
            {
                std::scoped_lock lock(samples->mutex);
                performance = { { "durationSeconds", duration / 1000 },
                                { "warmupSeconds", warmup / 1000 },
                                { "width", 1920 },
                                { "height", 1080 },
                                { "instances", 432 },
                                { "geometrySamples", 4 },
                                { "aoSamples", 32 },
                                { "shadowSize", 2048 },
                                { "bokeh", true },
                                { "smaa", false },
                                { "frameIntervals", distribution(samples->intervals) },
                                { "cpuSubmission", distribution(samples->cpu) },
                                { "gpuFrame", distribution(samples->gpu) },
                                { "atlasUploadBytes", samples->uploadBytes },
                                { "uniformUploadBytes", samples->uniformBytes },
                                { "resourceChanges", samples->resourceChanges } };
                performance["scope"] =
                        "Renderer-only camera motion; CPU submit excludes GUI/synchronize and GPU completion";
                performance["camera"] =
                        "SceneAdapter production browse target, span 7.33, distance 140";
                performance["driver"] =
                        "frameSwapped queued GUI update; event loop deadline; no polling/sleep";
                QJsonArray frameSamples, cpuSamples;
                for (double value : samples->intervals)
                    frameSamples.append(value);
                for (double value : samples->cpu)
                    cpuSamples.append(value);
                performance["frameIntervalSamplesMs"] = frameSamples;
                performance["cpuSubmissionSamplesMs"] = cpuSamples;
                if (shortProfile) {
                    const QStringList names{ "full",     "no-shadow", "no-AO",
                                             "no-bokeh", "no-shell",  "paper-only" };
                    QJsonObject stages;
                    for (int i = 0; i < 6; ++i)
                        stages[names[i]] = distribution(samples->stageGpu[i]);
                    performance["stageGpu"] = stages;
                    QJsonObject cadenceStages;
                    for (int i = 0; i < 6; ++i)
                        cadenceStages[names[i]] = distribution(samples->stageCadence[i]);
                    performance["stageCadence"] = cadenceStages;
                }
            }
            performance["presentPolicy"] = qEnvironmentVariableIntValue("QSG_NO_VSYNC") == 1
                    ? "Unthrottled native presentation; does not establish compositor/vsync cadence"
                    : "System vsync presentation";
            performance["gpuTimestampAvailable"] =
                    performance["gpuFrame"].toObject()["count"].toInt() > 0;
            const auto cadence = performance["frameIntervals"].toObject();
            const bool complete = cadence["count"].toInt() > 100
                    && performance["atlasUploadBytes"].toInteger() == 0
                    && performance["resourceChanges"].toInteger() == 0;
            performance["samplingComplete"] = complete;
            performance["gateEnforced"] = !recordPerformance;
            performance["passed"] = complete && cadence["p95Ms"].toDouble() <= 20
                    && cadence["over50Ms"].toInt() <= 1
                    && performance["atlasUploadBytes"].toInteger() == 0;
            report["performance"] = performance;
            report["functionalPassed"] = true;
            report["passed"] = complete && (recordPerformance || performance["passed"].toBool());
            report["acceptanceScope"] = recordPerformance
                    ? "Functional/effect/upload/resource checks; performance recorded only by R10D user authorization, not accepted as meeting the numeric target"
                    : "Functional checks and the historical renderer throughput gate";
            item->profiled = { };
            if (shortProfile) {
                report["passed"] = false;
                report["scope"] = "30s stage diagnostic; final replay/120s gate not executed";
            }
        } else {
            report["scope"] = "diagnostics only; final replay/performance not executed";
            report["passed"] = false;
        }
        QSaveFile file(output + "/runtime.json");
        report["aoOracle"] = aoOracle;
        check(file.open(QIODevice::WriteOnly), "Cannot save R10C report");
        file.write(QJsonDocument(report).toJson());
        check(file.commit(), "Cannot commit R10C report");
        check(!fullReplay || report["passed"].toBool(),
              "R10C 120s performance gate failed; inspect runtime.json");
        item->captured = { };
        window.hide();
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "R10C: %s\n", e.what());
        return 1;
    }
}
