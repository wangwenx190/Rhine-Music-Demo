#include "Require.h"
#include "ReviewApplication.h"
#include "RhiCaptureHarness.h"
#include "RhiReviewInput.h"
#include "PostprocessChecks.h"
#include "TransferChecks.h"
#include "OutputReference.h"
#include "GraphicsBackend.h"
#include <QGuiApplication>
#include <QFile>
#include <QJsonDocument>
#include "JsonFile.h"
#include <QDir>
#include <QPainter>
#include <mutex>
#include <cstdio>
#include <cmath>

using namespace Qt::StringLiterals;

using namespace yalahoo;
using namespace yalahoo::rhi;
using namespace yalahoo::review;

namespace {
void checkResolve(const Capture &capture, int samples, bool shells)
{
    const auto graph = capture.metadata[u"resolveGraph"_s].toArray();
    require(graph.size() == (shells ? 2 : 1), "Missing actual resolve targets");
    for (const auto &value : graph) {
        const auto target = value.toObject();
        require(target[u"samples"_s].toInt() == samples
                        && target[u"depthSamples"_s].toInt() == samples
                        && target[u"pipelineSamples"_s].toInt() == samples,
                "Geometry/depth sample count changed");
        const auto attachments = target[u"attachments"_s].toArray();
        require(attachments.size() == 3, "Review MRT attachment missing");
        for (int i = 0; i < attachments.size(); ++i) {
            const auto a = attachments[i].toObject();
            const int format = int(i == 0 ? QRhiTexture::RGBA16F : QRhiTexture::RGBA8);
            require(a[u"format"_s].toInt() == format && a[u"samples"_s].toInt() == samples
                            && a[u"width"_s].toInt() == capture.color.width()
                            && a[u"height"_s].toInt() == capture.color.height(),
                    "MSAA attachment lost format, size or samples");
            if (samples > 1)
                require(a[u"separateResolve"_s].toBool() && a[u"resolveSamples"_s].toInt() == 1
                                && a[u"resolveFormat"_s].toInt() == format
                                && a[u"resolveWidth"_s] == a[u"width"_s]
                                && a[u"resolveHeight"_s] == a[u"height"_s],
                        "MSAA resolve aliases, changes precision or has wrong dimensions");
            else
                require(a[u"resolveSamples"_s].toInt() == 0, "Single sample has redundant resolve");
        }
    }
    if (!shells)
        return;
    const auto opaque = graph[0].toObject()[u"attachments"_s].toArray();
    const auto composite = graph[1].toObject()[u"attachments"_s].toArray();
    for (int i = 0; i < opaque.size(); ++i) {
        const auto a = opaque[i].toObject(), b = composite[i].toObject();
        require((a[u"resourceId"_s] == b[u"resourceId"_s]) == (samples > 1),
                "Opaque/composite transient MSAA sharing is incorrect");
        if (samples > 1)
            require(a[u"resolveResourceId"_s] != b[u"resolveResourceId"_s],
                    "Opaque capture resolve aliases composite output");
    }
}
void checkTransientInventory(const QJsonObject &resources, QSize size, int samples, bool diagnostic)
{
    require(resources[u"outputHasDepthStencil"_s].isBool()
                    && !resources[u"outputHasDepthStencil"_s].toBool(),
            "Fullscreen output allocated an unused depth/stencil attachment");
    qint64 actualBytes = 0, plannedBytes = 0;
    for (const auto &entry : resources[u"entries"_s].toArray()) {
        const auto row = entry.toObject();
        if (row[u"name"_s].toString().contains("-msaa-"_L1))
            actualBytes += row[u"estimatedBytes"_s].toInteger();
    }
    for (const auto &entry : resources[u"preallocationPlan"_s].toObject()[u"entries"_s].toArray()) {
        const auto row = entry.toObject();
        if (row[u"name"_s].toString().endsWith("-msaa"_L1)
            && row[u"name"_s].toString() != "qt-item-msaa"_L1)
            plannedBytes += row[u"estimatedBytes"_s].toInteger();
    }
    // One RGBA16F MSAA target, plus two RGBA8 channels only in diagnostic sessions.
    const qint64 expected = samples > 1
            ? qint64(size.width()) * size.height() * samples * (diagnostic ? 16 : 8)
            : 0;
    require(actualBytes == expected && plannedBytes == expected,
            "Actual/preallocation inventories do not count shared MSAA exactly once");
}
QJsonObject checkProductionAttachments(Input input, QSize size)
{
    struct Snapshot final
    {
        std::mutex mutex;
        QJsonObject resources;
    };
    auto snapshot = std::make_shared<Snapshot>();
    RhiCaptureHarness driver;
    driver.item->captured = { };
    driver.item->profiled = [snapshot](QJsonObject sample) {
        std::scoped_lock lock(snapshot->mutex);
        snapshot->resources = sample[u"resources"_s].toObject();
    };
    input.profile = true;
    input.rawOutput = false;
    driver.replay(1, [&](int) {
        driver.submitWithoutCapture(input, size);
    });
    QJsonObject resources;
    {
        std::scoped_lock lock(snapshot->mutex);
        resources = snapshot->resources;
    }
    checkTransientInventory(resources, size, input.captureSamples, false);
    const auto output = driver.surface->grab();
    require(output.size() == size, "Production attachment output missing");
    const auto expected = outputReference(input.background, input.exposure);
    const auto pixel = output.pixelColor(0, 0);
    const std::array<double, 3> actual{ pixel.redF(), pixel.greenF(), pixel.blueF() };
    for (int channel = 0; channel < 3; ++channel) {
        const auto linear = expected[channel];
        const double srgb =
                linear <= .0031308 ? linear * 12.92 : 1.055 * std::pow(linear, 1 / 2.4) - .055;
        require(std::abs(actual[channel] - srgb) <= 1.0 / 255 && pixel.alpha() == 255,
                "Production resolve changed final background color");
    }
    require(verification::windowPolicySatisfied(*driver.window), "Production window policy failed");
    return resources;
}
void checkUniformReuse(RhiCaptureHarness &driver, Input input, QSize size, const Capture &initial)
{
    const auto stable = driver.capture(input, size);
    require(stable.metadata[u"postUniformUploadBytes"_s].toInteger() == 0
                    && stable.postprocessed == initial.postprocessed,
            "Unchanged combined postprocess uniforms were uploaded or changed output");
    const auto original = input;
    input.focus += 5;
    input.aperture *= 2;
    input.scene.projection.scale(1.1f, .9f);
    const auto changed = driver.capture(input, size);
    const auto passes =
            changed.metadata[u"postprocessGraph"_s].toObject()[u"passes"_s].toArray().size();
    require(changed.metadata[u"postUniformUploadBytes"_s].toInteger() == passes * 80
                    && changed.metadata[u"postprocessRevision"_s]
                            == stable.metadata[u"postprocessRevision"_s],
            "Changed postprocess parameters were not uploaded without rebuilding");
    RhiCaptureHarness fresh;
    const auto rebuilt = fresh.capture(input, size);
    require(changed.postprocessed == rebuilt.postprocessed,
            "Updated cached uniforms differ from fresh postprocess resources");
    const auto restored = driver.capture(original, size);
    require(restored.postprocessed == initial.postprocessed,
            "Reverting postprocess parameters retained stale uniforms");
}
void checkOutputTransitions(RhiCaptureHarness &driver, Input input, QSize size)
{
    input.sceneOccluded = true;
    driver.replay(1, [&](int) {
        driver.submitWithoutCapture(input, size);
    });
    const auto cleared = driver.surface->grab();
    require(cleared.size() == size, "Occluded output target was not initialized");
    require(cleared.pixelColor(size.width() / 2, size.height() / 2).alpha() == 0,
            "Occluded output retained stale scene color");
    input.sceneOccluded = false;
    for (bool hdr : { true, false }) {
        driver.item->setHdrActive(hdr);
        input.hdrActive = hdr;
        const auto capture = driver.capture(input, size);
        checkTransientInventory(capture.metadata[u"resources"_s].toObject(), size,
                                input.captureSamples, true);
        const auto pixel = driver.surface->grab().pixelColor(0, 0);
        require(pixel.alpha() == 255, "Output format change did not restore opaque scene color");
    }
    driver.item->setSampleCount(4);
    const auto multisample = driver.capture(input, size);
    const auto resources = multisample.metadata[u"resources"_s].toObject();
    require(resources[u"outputSamples"_s].toInt() == 4,
            "Color-only output lost Qt's requested MSAA");
    checkTransientInventory(resources, size, input.captureSamples, true);
    driver.item->setSampleCount(1);
    const auto restored = driver.capture(input, size);
    require(restored.metadata[u"resources"_s].toObject()[u"outputSamples"_s].toInt() == 1,
            "Color-only output retained stale MSAA attachments");
}
QJsonObject checkAttachments(const QJsonArray &sourceCases)
{
    RhiCaptureHarness driver;
    auto input = fixture(sourceCases[3].toObject());
    input.shadows = false;
    input.postprocess = true;
    input.aoSamples = 32;
    input.bokeh = input.smaa = true;
    QJsonArray states;
    // Resize and both single/multisample paths exercise reuse without unrelated stress.
    for (const QSize size : { QSize(321, 241), QSize(241, 321) }) {
        input.scene.projection.setToIdentity();
        input.scene.projection.perspective(32, float(size.width()) / size.height(), 5, 300);
        for (int samples : { 1, 4 }) {
            input.captureSamples = samples;
            const auto capture = driver.capture(input, size);
            checkResolve(capture, samples, true);
            checkPostprocessGraph(input, capture);
            checkTransientInventory(capture.metadata[u"resources"_s].toObject(), size, samples,
                                    true);
            if (size == QSize(321, 241) && samples == 1) {
                checkUniformReuse(driver, input, size, capture);
                checkOutputTransitions(driver, input, size);
            }
            states.append(QJsonObject{
                    { u"diagnostic"_s, capture.metadata },
                    { u"productionResources"_s, checkProductionAttachments(input, size) } });
        }
    }
    return { { u"passed"_s, true },
             { u"scope"_s,
               u"transient-attachments-only; no performance sampling or lifecycle stress"_s },
             { u"states"_s, states },
             { u"offscreen"_s, driver.surface->diagnostics() },
             { u"windowPolicy"_s, verification::windowPolicy(*driver.window) } };
}
auto runPostprocessChecks(int &argc, char **&argv)
{
    const bool attachmentsOnly = argc == 3 && QByteArray(argv[2]) == "--attachments-only"_ba;
    if (argc != 2 && !attachmentsOnly)
        return 2;
    if (qEnvironmentVariableIntValue("YALAHOO_VERIFY_VISIBLE") == 1)
        OffscreenSurface::Enabled = false;
    try {
        const QString output = QString::fromLocal8Bit(argv[1]);
        require(QDir().mkpath(output), "Cannot create diagnostic output");
        const auto original = readFixture((QString::fromUtf8(FIXTURE_ROOT))
                                          + "/tests/fixtures/render-backend/visual.json"_L1);
        const auto sourceCases = original[u"cases"_s].toArray();
        if (attachmentsOnly) {
            const auto report = checkAttachments(sourceCases);
            require(yalahoo::writeJsonFile(output + "/runtime.json"_L1, QJsonDocument(report)),
                    "Cannot commit attachment report");
            std::puts("Shared MSAA, independent resolves and production attachments passed.");
            return 0;
        }
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
                    require(first.metadata[u"resourceGeneration"_s]
                                            == stable.metadata[u"resourceGeneration"_s]
                                    && first.metadata[u"postprocessGraph"_s]
                                            == stable.metadata[u"postprocessGraph"_s]
                                    && first.postprocessed == stable.postprocessed,
                            "Stable resolve/postprocess changed or rebuilt");
                    require(stable.metadata[u"postUniformUploadBytes"_s].toInteger() == 0,
                            "Steady postprocess repeated unchanged uniforms");
                    if (size == QSize(640, 480) && samples == 4)
                        require(stable.postprocessed.save(output + u"/combined-%1.png"_s.arg(mode)),
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
        require(!finalImage.isNull() && finalImage.save(output + "/final-output.png"_L1),
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
        require(finalError <= 1.0 / 255 && actual.alpha() == 255,
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
        require(active.metadata[u"resourceGeneration"_s]
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
            require(!name.startsWith(u"post-"_s) && !name.startsWith(u"smaa-"_s)
                            && !name.startsWith(u"ao-geometry"_s)
                            && !name.startsWith(u"dof-geometry"_s),
                    "Disabled channel retained resources");
        }
        input.postprocess = input.bokeh = input.smaa = true;
        input.aoSamples = 32;
        {
            driver.rebuild();
            const auto result = driver.capture(input, { 640, 480 });
            require(driver.rebuilt(), "No actual scene-graph invalidation");
            checkResolve(result, 4, true);
            checkPostprocessGraph(input, result);
            require(!result.postprocessed.isNull(), "Rebuilt output missing");
            rebuilds.append(result.metadata);
        }
        auto coverInput = fixture(sourceCases[25].toObject());
        const auto firstCover = driver.capture(coverInput, { 960, 720 }, true);
        const auto replayCover = driver.capture(coverInput, { 960, 720 }, true);
        require(firstCover.metadata[u"coverSnapshotCopies"_s]
                                == replayCover.metadata[u"coverSnapshotCopies"_s]
                        && firstCover.atlas == replayCover.atlas,
                "Unchanged cover copied or changed on synchronization");
        coverInput.cover.fill(Qt::blue);
        const auto changedCover = driver.capture(coverInput, { 960, 720 }, true);
        require(changedCover.metadata[u"coverSnapshotCopies"_s].toInteger()
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
        require(rejectedFallback, "Fallback without primary leases was accepted");
        const auto initial = driver.capture(atlas, { 960, 720 }, true);
        for (const auto &entry : initial.metadata[u"effectiveCoverRegions"_s].toArray())
            require(entry.toObject()[u"lease"_s].isNull(), "Absent atlas lease fabricated a value");
        initial.composite.save(output + "/atlas-432-composite.png"_L1);
        require(initial.atlas == page, "Initialized atlas GPU bytes differ");
        auto native = initial.metadata[u"nativeAtlasTransfers"_s].toObject();
        const bool nativeProof = native[u"available"_s].toBool();
        checkAtlasTransfer(driver.window->rendererInterface()->graphicsApi(), initial.metadata,
                           16777216);
        atlas.initializingAtlas = false;
        atlas.atlasUploads.clear();
        const auto stable = driver.capture(atlas, { 960, 720 }, true);
        require(stable.metadata[u"atlasUploadBytes"_s].toInteger() == 0
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
            require(result.metadata[u"generation"_s] == stable.metadata[u"generation"_s],
                    "Entity count/visibility recreated the render graph");
            require(result.metadata[u"uniformUploadBytes"_s].toInteger()
                            <= result.metadata[u"uniformCapacityBytes"_s].toInteger(),
                    "Uniform prefix exceeds its fixed capacity");
            require(result.atlas == stable.atlas, "Entity count changed atlas pixels");
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
        require(maximum.metadata[u"visibleEntities"_s].toInt() == 432
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
        require((nativeProof ? native[u"destinationBytes"_s].toInteger()
                             : tile.metadata[u"atlasUploadBytes"_s].toInteger())
                                == 262144
                        && (!nativeProof || native[u"regions"_s].toArray().size() == 1),
                "One tile caused a native whole-page transfer");
        QImage expected = page.copy();
        {
            QPainter painter(&expected);
            painter.drawImage(0, 0, replacement);
        }
        require(tile.atlas == expected && tile.postprocessed != initial.postprocessed,
                "Atlas tile changed wrong GPU region or did not reach instances");
        atlas.atlasUploads[0].serial = 3;
        atlas.atlasUploads[0].pixels = page.copy(0, 0, 256, 256);
        const auto revert = driver.capture(atlas, { 960, 720 }, true);
        require(revert.atlas == initial.atlas && revert.postprocessed == initial.postprocessed,
                "Atlas revert changed pixels");
        atlas.atlasUploads.clear();
        std::reverse(atlas.entities.begin(), atlas.entities.end());
        std::reverse(atlas.entityIds.begin(), atlas.entityIds.end());
        std::reverse(atlas.coverTiles.begin(), atlas.coverTiles.end());
        const auto reverse = driver.capture(atlas, { 960, 720 }, true);
        require(reverse.postprocessed == initial.postprocessed
                        && reverse.metadata[u"sortedShells"_s]
                                == initial.metadata[u"sortedShells"_s],
                "432-instance order/atlas assignment changed");
        // Target rebuild must retain atlas resource and must not upload it again.
        const auto resized = driver.capture(atlas, { 1280, 720 }, true);
        require(resized.atlas == page && resized.metadata[u"atlasUploadBytes"_s].toInteger() == 0
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
        require(rebuilt.atlas == page && rebuilt.metadata[u"atlasRejected"_s].toInteger() > 0,
                "Destroyed atlas accepted old generation");
        atlas.initializingAtlas = false;
        atlas.atlasUploads.clear();
        for (int slot = 0; slot < 9; ++slot)
            atlas.atlasUploads.append({ 2, 5, 2, slot, replacement });
        const auto budget = driver.capture(atlas, { 960, 720 }, true);
        require((nativeProof ? budget.metadata[u"nativeAtlasTransfers"_s]
                                       .toObject()[u"destinationBytes"_s]
                                       .toInteger()
                             : budget.metadata[u"atlasUploadBytes"_s].toInteger())
                                == 2097152
                        && budget.metadata[u"atlasPending"_s].toInt() == 1,
                "Steady queue did not defer the ninth tile");
        atlas.atlasUploads.clear();
        const auto drained = driver.capture(atlas, { 960, 720 }, true);
        require(drained.metadata[u"atlasPending"_s].toInt() == 0,
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
        QJsonObject report{ { u"qt"_s, QString::fromUtf8(qVersion()) },
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
                            { u"offscreen"_s, driver.surface->diagnostics() },
                            { u"windowPolicy"_s, verification::windowPolicy(*driver.window) } };
        require(yalahoo::writeJsonFile(output + "/runtime.json"_L1, QJsonDocument(report)),
                "Cannot commit report");
        std::puts("MSAA/resolve, combined postprocess and bounded atlas checks passed.");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "RHI postprocess: %s\n", error.what());
        return 1;
    }
}
}
int main(int argc, char **argv)
{
    return yalahoo::review::runApplication(argc, argv, [&](QGuiApplication &) {
        return runPostprocessChecks(argc, argv);
    });
}
