#include "RhiCaptureHarness.h"
#include "RhiReviewInput.h"
#include "PostprocessChecks.h"
#include "OutputReference.h"
#include "GraphicsBackend.h"
#include <QGuiApplication>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QDir>
#include <QPainter>
#include <QElapsedTimer>
#include <mutex>
#include <cstdio>
#include <cmath>

using namespace Qt::StringLiterals;

using namespace yalahoo;
using namespace yalahoo::rhi;
using namespace yalahoo::review;
namespace {
void check(bool condition, const char *message)
{
    RhiCaptureHarness::require(condition, message);
}
QJsonObject read(const QString &path)
{
    QFile file(path);
    check(file.open(QIODevice::ReadOnly), "Missing source mathematical input");
    const auto doc = QJsonDocument::fromJson(file.readAll());
    check(doc.isObject(), "Invalid source mathematical input");
    return doc.object();
}
void checkResolve(const Capture &capture, int samples, bool shells)
{
    const auto graph = capture.metadata[u"resolveGraph"_s].toArray();
    check(graph.size() == (shells ? 2 : 1), "Missing actual resolve targets");
    for (const auto &value : graph) {
        const auto target = value.toObject();
        check(target[u"samples"_s].toInt() == samples
                      && target[u"depthSamples"_s].toInt() == samples
                      && target[u"pipelineSamples"_s].toInt() == samples,
              "Geometry/depth sample count changed");
        const auto attachments = target[u"attachments"_s].toArray();
        check(attachments.size() == 3, "Review MRT attachment missing");
        for (int i = 0; i < attachments.size(); ++i) {
            const auto a = attachments[i].toObject();
            const int format = int(i == 0 ? QRhiTexture::RGBA16F : QRhiTexture::RGBA8);
            check(a[u"format"_s].toInt() == format && a[u"samples"_s].toInt() == samples
                          && a[u"width"_s].toInt() == capture.color.width()
                          && a[u"height"_s].toInt() == capture.color.height(),
                  "MSAA attachment lost format, size or samples");
            if (samples > 1)
                check(a[u"separateResolve"_s].toBool() && a[u"resolveSamples"_s].toInt() == 1
                              && a[u"resolveFormat"_s].toInt() == format
                              && a[u"resolveWidth"_s] == a[u"width"_s]
                              && a[u"resolveHeight"_s] == a[u"height"_s],
                      "MSAA resolve aliases, changes precision or has wrong dimensions");
            else
                check(a[u"resolveSamples"_s].toInt() == 0, "Single sample has redundant resolve");
        }
    }
}
QJsonObject distribution(QVector<double> values)
{
    check(!values.isEmpty(), "Empty local CPU submission sample");
    std::sort(values.begin(), values.end());
    return { { u"count"_s, values.size() },
             { u"p50Ms"_s, values[(values.size() - 1) / 2] },
             { u"p95Ms"_s, values[int((values.size() - 1) * .95)] },
             { u"maxMs"_s, values.last() } };
}
}
int main(int argc, char **argv)
{
    verification::prepareApplication();
    if (!selectGraphicsBackend())
        return 2;
    QGuiApplication app(argc, argv);
    if (!yalahoo::probeOptionalGraphicsBackend())
        return 22;
    if (argc == 2 && QByteArray(argv[1]) == "--adapters"_ba) {
        std::printf("%s\n", QJsonDocument(adapterInventory()).toJson().constData());
        return 0;
    }
    const bool profile = argc == 3 && QByteArray(argv[2]) == "--profile"_ba;
    if (argc != 2 && !profile)
        return 2;
    if (qEnvironmentVariableIntValue("YALAHOO_VERIFY_VISIBLE") == 1)
        OffscreenSurface::Enabled = false;
    try {
        const QString output = QString::fromLocal8Bit(argv[1]);
        check(QDir().mkpath(output), "Cannot create diagnostic output");
        const auto original = read((QString::fromUtf8(FIXTURE_ROOT))
                                   + "/tests/fixtures/render-backend/visual.json"_L1);
        const auto sourceCases = original[u"cases"_s].toArray();
        RhiCaptureHarness driver;
        QJsonArray states, rebuilds;
        auto input = fixture(sourceCases[3].toObject());
        input.shadows = false;
        input.postprocess = true;
        input.rawOutput = true;
        input.focus = 25;
        input.aperture = .0003f;
        // Independent odd/portrait targets exercise resolve without changing production scale policy.
        for (const QSize size : { QSize(321, 241), QSize(241, 321), QSize(640, 480) }) {
            input.scene.projection.setToIdentity();
            input.scene.projection.perspective(32, float(size.width()) / size.height(), 5, 300);
            for (int samples : { 1, 4 }) {
                input.captureSamples = samples;
                for (int mode = 0; mode < 8; ++mode) {
                    input.aoSamples = mode & 1 ? 32 : 0;
                    input.bokeh = mode & 2;
                    input.smaa = mode & 4;
                    const auto first = driver.capture(input, size);
                    const auto stable = driver.capture(input, size);
                    checkResolve(stable, samples, true);
                    checkPostprocessGraph(input, stable);
                    check(first.metadata[u"resourceGeneration"_s]
                                          == stable.metadata[u"resourceGeneration"_s]
                                  && first.metadata[u"postprocessGraph"_s]
                                          == stable.metadata[u"postprocessGraph"_s]
                                  && first.postprocessed == stable.postprocessed,
                          "Stable resolve/postprocess changed or rebuilt");
                    const int passCount = stable.metadata[u"postprocessGraph"_s]
                                                  .toObject()[u"passes"_s]
                                                  .toArray()
                                                  .size();
                    check(stable.metadata[u"postUniformUploadBytes"_s].toInteger()
                                  == passCount * 80,
                          "Steady postprocess uploaded beyond dynamic prefixes");
                    if (size == QSize(640, 480) && samples == 4)
                        check(stable.postprocessed.save(output + u"/combined-%1.png"_s.arg(mode)),
                              "Cannot save actual combined output");
                    states.append(
                            QJsonObject{ { u"mode"_s, mode }, { u"metadata"_s, stable.metadata } });
                }
            }
        }
        // Check the real final compositor against source color math, independent of its uniforms.
        input.rawOutput = false;
        const auto finalCapture = driver.capture(input, { 640, 480 });
        checkPostprocessGraph(input, finalCapture);
        const auto finalImage = driver.surface->grab();
        check(!finalImage.isNull() && finalImage.save(output + "/final-output.png"_L1),
              "Final compositor output missing");
        const auto expectedOutput = outputReference(input.background, input.exposure);
        const auto actual = finalImage.pixelColor(0, 0);
        double finalError = 0;
        for (int channel = 0; channel < 3; ++channel) {
            const double linear = expectedOutput[channel];
            const double srgb =
                    linear <= .0031308 ? linear * 12.92 : 1.055 * std::pow(linear, 1 / 2.4) - .055;
            const double rendered = channel == 0 ? actual.redF()
                    : channel == 1               ? actual.greenF()
                                                 : actual.blueF();
            finalError = std::max(finalError, std::abs(rendered - srgb));
        }
        check(finalError <= 1.0 / 255 && actual.alpha() == 255,
              "Final output omitted or repeated ACES/sRGB conversion");
        input.rawOutput = true;
        // Disabling releases graph resources; numerical changes reuse existing compatible attachments.
        input.aoSamples = 32;
        input.bokeh = input.smaa = true;
        const auto active = driver.capture(input, { 640, 480 });
        input.focus = 30;
        input.aperture = .0008f;
        input.exposure = 1.1f;
        const auto numerical = driver.capture(input, { 640, 480 });
        check(active.metadata[u"resourceGeneration"_s]
                              == numerical.metadata[u"resourceGeneration"_s]
                      && active.metadata[u"postprocessGraph"_s]
                              == numerical.metadata[u"postprocessGraph"_s],
              "Numerical state rebuilt compatible targets");
        input.postprocess = false;
        input.aoSamples = 0;
        input.bokeh = input.smaa = false;
        const auto disabled = driver.capture(input, { 640, 480 });
        checkPostprocessGraph(input, disabled);
        for (const auto &entry :
             disabled.metadata[u"resources"_s].toObject()[u"entries"_s].toArray()) {
            const auto name = entry.toObject()[u"name"_s].toString();
            check(!name.startsWith(u"post-"_s) && !name.startsWith(u"smaa-"_s)
                          && !name.startsWith(u"ao-geometry"_s)
                          && !name.startsWith(u"dof-geometry"_s),
                  "Disabled channel retained resources");
        }
        input.postprocess = input.bokeh = input.smaa = true;
        input.aoSamples = 32;
        for (int cycle = 0; cycle < 10; ++cycle) {
            driver.rebuild();
            const auto result = driver.capture(input, { 640, 480 });
            check(driver.rebuilt(), "No actual scene-graph invalidation");
            checkResolve(result, 4, true);
            checkPostprocessGraph(input, result);
            check(!result.postprocessed.isNull(), "Rebuilt output missing");
            rebuilds.append(result.metadata);
        }
        auto coverInput = fixture(sourceCases[25].toObject());
        const auto firstCover = driver.capture(coverInput, { 960, 720 }, true);
        const auto replayCover = driver.capture(coverInput, { 960, 720 }, true);
        check(firstCover.metadata[u"coverSnapshotCopies"_s]
                              == replayCover.metadata[u"coverSnapshotCopies"_s]
                      && firstCover.atlas == replayCover.atlas,
              "Unchanged cover copied or changed on synchronization");
        coverInput.cover.fill(Qt::blue);
        const auto changedCover = driver.capture(coverInput, { 960, 720 }, true);
        check(changedCover.metadata[u"coverSnapshotCopies"_s].toInteger()
                              == replayCover.metadata[u"coverSnapshotCopies"_s].toInteger() + 1
                      && changedCover.atlas.pixelColor(0, 0) == QColor(Qt::blue),
              "New cover revision was not copied/uploaded");
        auto atlas = fixture(sourceCases[25].toObject());
        atlas.postprocess = true;
        atlas.aoSamples = 32;
        atlas.bokeh = true;
        atlas.focus = float(sourceCases[25].toObject()[u"focus"_s].toDouble());
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
        auto invalidFallback = atlas;
        invalidFallback.fallbackCoverTiles = atlas.coverTiles;
        invalidFallback.fallbackCoverLeases.fill(1, atlas.entities.size());
        bool rejectedFallback = false;
        try {
            driver.item->submit(invalidFallback, { 960, 720 });
        } catch (const std::exception &) {
            rejectedFallback = true;
        }
        check(rejectedFallback, "Fallback without primary leases was accepted");
        const auto initial = driver.capture(atlas, { 960, 720 }, true);
        for (const auto &entry : initial.metadata[u"effectiveCoverRegions"_s].toArray())
            check(entry.toObject()[u"lease"_s].isNull(), "Absent atlas lease fabricated a value");
        initial.composite.save(output + "/atlas-432-composite.png"_L1);
        check(initial.atlas == page, "Initialized atlas GPU bytes differ");
        auto native = initial.metadata[u"nativeAtlasTransfers"_s].toObject();
        const bool nativeProof = native[u"available"_s].toBool();
#if defined(Q_OS_WIN)
        check(nativeProof, "Windows D3D11 native transfer proof unavailable");
#endif
        check((nativeProof ? native[u"destinationBytes"_s].toInteger()
                           : initial.metadata[u"atlasUploadBytes"_s].toInteger())
                      == 16777216,
              "Initialization native transfer bytes unavailable/over budget");
        atlas.initializingAtlas = false;
        atlas.atlasUploads.clear();
        const auto stable = driver.capture(atlas, { 960, 720 }, true);
        check(stable.metadata[u"atlasUploadBytes"_s].toInteger() == 0
                      && (!nativeProof
                          || stable.metadata[u"nativeAtlasTransfers"_s]
                                          .toObject()[u"destinationBytes"_s]
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
            const auto result = driver.capture(bounded, { 960, 720 }, true);
            check(result.metadata[u"generation"_s] == stable.metadata[u"generation"_s],
                  "Entity count/visibility recreated the render graph");
            check(result.metadata[u"uniformUploadBytes"_s].toInteger()
                          <= result.metadata[u"uniformCapacityBytes"_s].toInteger(),
                  "Uniform prefix exceeds its fixed capacity");
            check(result.atlas == stable.atlas, "Entity count changed atlas pixels");
            capacityCases.append(QJsonObject{
                    { u"instances"_s, count },
                    { u"generation"_s, result.metadata[u"generation"_s] },
                    { u"uploadBytes"_s, result.metadata[u"uniformUploadBytes"_s] },
                    { u"capacityBytes"_s, result.metadata[u"uniformCapacityBytes"_s] } });
        }
        auto fullVisibility = atlas;
        fullVisibility.scene.view.setToIdentity();
        fullVisibility.scene.view.lookAt({ 0, 2, 100 }, { 0, 2, 0 }, { 0, 1, 0 });
        fullVisibility.scene.projection.setToIdentity();
        fullVisibility.scene.projection.ortho(-30, 30, -30, 30, .1f, 300);
        const auto maximum = driver.capture(fullVisibility, { 960, 720 }, true);
        check(maximum.metadata[u"visibleEntities"_s].toInt() == 432
                      && maximum.metadata[u"uniformUploadBytes"_s].toInteger()
                              <= maximum.metadata[u"uniformCapacityBytes"_s].toInteger()
                      && maximum.metadata[u"generation"_s] == stable.metadata[u"generation"_s],
              "Fully visible pool exceeded capacity or rebuilt the render graph");
        capacityCases.append(
                QJsonObject{ { u"instances"_s, 432 },
                             { u"allVisible"_s, true },
                             { u"generation"_s, maximum.metadata[u"generation"_s] },
                             { u"uploadBytes"_s, maximum.metadata[u"uniformUploadBytes"_s] },
                             { u"capacityBytes"_s, maximum.metadata[u"uniformCapacityBytes"_s] } });
        QImage replacement(256, 256, QImage::Format_RGBA8888);
        replacement.fill(QColor(230, 30, 90));
        atlas.atlasUploads = { { 1, 2, 2, 0, replacement } };
        const auto tile = driver.capture(atlas, { 960, 720 }, true);
        native = tile.metadata[u"nativeAtlasTransfers"_s].toObject();
        check((nativeProof ? native[u"destinationBytes"_s].toInteger()
                           : tile.metadata[u"atlasUploadBytes"_s].toInteger())
                              == 262144
                      && (!nativeProof || native[u"regions"_s].toArray().size() == 1),
              "One tile caused a native whole-page transfer");
        QImage expected = page.copy();
        {
            QPainter painter(&expected);
            painter.drawImage(0, 0, replacement);
        }
        check(tile.atlas == expected && tile.postprocessed != initial.postprocessed,
              "Atlas tile changed wrong GPU region or did not reach instances");
        atlas.atlasUploads[0].serial = 3;
        atlas.atlasUploads[0].pixels = page.copy(0, 0, 256, 256);
        const auto revert = driver.capture(atlas, { 960, 720 }, true);
        check(revert.atlas == initial.atlas && revert.postprocessed == initial.postprocessed,
              "Atlas revert changed pixels");
        atlas.atlasUploads.clear();
        std::reverse(atlas.entities.begin(), atlas.entities.end());
        std::reverse(atlas.entityIds.begin(), atlas.entityIds.end());
        std::reverse(atlas.coverTiles.begin(), atlas.coverTiles.end());
        const auto reverse = driver.capture(atlas, { 960, 720 }, true);
        check(reverse.postprocessed == initial.postprocessed
                      && reverse.metadata[u"sortedShells"_s] == initial.metadata[u"sortedShells"_s],
              "432-instance order/atlas assignment changed");
        // Target rebuild must retain atlas resource and must not upload it again.
        const auto resized = driver.capture(atlas, { 1280, 720 }, true);
        check(resized.atlas == page && resized.metadata[u"atlasUploadBytes"_s].toInteger() == 0
                      && resized.metadata[u"nativeAtlasTransfers"_s]
                                      .toObject()[u"destinationBytes"_s]
                                      .toInteger()
                              == 0,
              "Resize reuploaded/lost atlas");
        atlas.atlasGeneration = 2;
        atlas.initializingAtlas = true;
        for (int slot = 0; slot < 64; ++slot)
            atlas.atlasUploads.append(
                    { 2, 4, 1, slot, page.copy((slot % 8) * 256, (slot / 8) * 256, 256, 256) });
        atlas.atlasUploads.append({ 1, 99, 100, 0, replacement });
        const auto rebuilt = driver.capture(atlas, { 960, 720 }, true);
        check(rebuilt.atlas == page && rebuilt.metadata[u"atlasRejected"_s].toInteger() > 0,
              "Destroyed atlas accepted old generation");
        atlas.initializingAtlas = false;
        atlas.atlasUploads.clear();
        for (int slot = 0; slot < 9; ++slot)
            atlas.atlasUploads.append({ 2, 5, 2, slot, replacement });
        const auto budget = driver.capture(atlas, { 960, 720 }, true);
        check((nativeProof ? budget.metadata[u"nativeAtlasTransfers"_s]
                                     .toObject()[u"destinationBytes"_s]
                                     .toInteger()
                           : budget.metadata[u"atlasUploadBytes"_s].toInteger())
                              == 2097152
                      && budget.metadata[u"atlasPending"_s].toInt() == 1,
              "Steady queue did not defer the ninth tile");
        atlas.atlasUploads.clear();
        const auto drained = driver.capture(atlas, { 960, 720 }, true);
        check(drained.metadata[u"atlasPending"_s].toInt() == 0,
              "Deferred atlas queue did not drain");
        initial.postprocessed.save(output + "/atlas-432-initial.png"_L1);
        tile.postprocessed.save(output + "/atlas-432-tile.png"_L1);
        QJsonObject atlasReport{
            { u"instances"_s, 432 },           { u"initial"_s, initial.metadata },
            { u"stable"_s, stable.metadata },  { u"tile"_s, tile.metadata },
            { u"revert"_s, revert.metadata },  { u"reverse"_s, reverse.metadata },
            { u"resize"_s, resized.metadata }, { u"rebuild"_s, rebuilt.metadata },
            { u"gpuPageExact"_s, true },       { u"unchangedOutsideTile"_s, true },
            { u"stablePixelsExact"_s, true },  { u"revertPixelsExact"_s, true }
        };
        QJsonArray costs;
        if (profile) {
            // Local CPU command submission only. RenderControl has no physical presentation/VSYNC.
            struct Samples
            {
                std::mutex mutex;
                QVector<double> cpu;
            };
            auto samples = std::make_shared<Samples>();
            driver.item->profiled = [samples](QJsonObject sample) {
                std::scoped_lock lock(samples->mutex);
                samples->cpu.append(sample[u"cpuSubmitMs"_s].toDouble());
            };
            driver.item->captured = { };
            atlas.profile = true;
            atlas.atlasUploads.clear();
            atlas.shadows = true;
            atlas.rawOutput = false;
            atlas.smaa = true;
            for (int run = 0; run < 3; ++run) {
                for (int stage = 0; stage < 6; ++stage) {
                    auto local = atlas;
                    local.shadows = stage != 1 && stage != 5;
                    local.aoSamples = stage == 2 || stage == 5 ? 0 : 32;
                    local.bokeh = stage != 3 && stage != 5;
                    local.shells = stage != 4 && stage != 5;
                    local.postprocess = stage != 5;
                    const auto advance = [&](int) {
                        driver.submitWithoutCapture(local, { 960, 720 });
                    };
                    driver.replay(10, advance);
                    {
                        std::scoped_lock lock(samples->mutex);
                        samples->cpu.clear();
                    }
                    driver.replay(60, advance);
                    std::scoped_lock lock(samples->mutex);
                    costs.append(QJsonObject{ { u"run"_s, run },
                                              { u"stage"_s, stage },
                                              { u"cpuSubmission"_s, distribution(samples->cpu) } });
                }
            }
            driver.item->profiled = { };
        }
        QJsonObject report{
            { u"round"_s, u"R19D4B2"_s },
            { u"qt"_s, QString::fromUtf8(qVersion()) },
            { u"configuration"_s, u"Release"_s },
            { u"passed"_s, true },
            { u"states"_s, states },
            { u"rebuilds"_s, rebuilds },
            { u"atlas"_s, atlasReport },
            { u"capacityCases"_s, capacityCases },
            { u"steadyBudget"_s,
              QJsonObject{ { u"firstFrame"_s, budget.metadata },
                           { u"drainedFrame"_s, drained.metadata } } },
            { u"nativeTransferByteProof"_s, nativeProof },
            { u"coverSnapshotReuse"_s, true },
            { u"finalOutputError"_s, finalError },
            { u"localCosts"_s, costs },
            { u"costScope"_s,
              u"CPU submission only; renderer-only anonymous pool. Three independent batches per stage; no GPU/display/VSYNC or full-product performance claim."_s },
            { u"gpuCompletedMs"_s, QJsonValue() },
            { u"displayLatencyMs"_s, QJsonValue() },
            { u"offscreen"_s, driver.surface->diagnostics() },
            { u"windowPolicy"_s, verification::windowPolicy(*driver.window) }
        };
        QSaveFile file(output + "/runtime.json"_L1);
        check(file.open(QIODevice::WriteOnly), "Cannot write report");
        file.write(QJsonDocument(report).toJson());
        check(file.commit(), "Cannot commit report");
        std::puts("MSAA/resolve, combined postprocess and bounded atlas checks passed.");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "RHI postprocess: %s\n", error.what());
        return 1;
    }
}
