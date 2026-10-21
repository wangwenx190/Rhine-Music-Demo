#include "ReviewSupport.h"
#include "ReviewApplication.h"
#include "SceneAdapter.h"
#include "MotionController.h"
#include "SelectionController.h"
#include "CoverCache.h"
#include "CoverSamplingChecks.h"
#include <QScopeGuard>
#include "RhiCaptureHarness.h"
#include "RhiProductionFixture.h"
#include "PostprocessChecks.h"
#include "GraphicsBackend.h"
#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include "JsonFile.h"
#include <QSet>
#include <cmath>
#include <cstdio>

using namespace Qt::StringLiterals;

using namespace yalahoo;
using namespace yalahoo::rhi;
using namespace yalahoo::review;

namespace {
QJsonArray pair(QSize size)
{
    return { size.width(), size.height() };
}
QJsonArray matrix(const QMatrix4x4 &value)
{
    QJsonArray result;
    for (int i = 0; i < 16; ++i)
        result.append(value.constData()[i]);
    return result;
}
QJsonObject sourceState(const Input &input)
{
    QJsonArray ids, models;
    for (auto id : input.entityIds)
        ids.append(qint64(id));
    for (const auto &model : input.entities)
        models.append(matrix(model));
    return { { u"view"_s, matrix(input.scene.view) },
             { u"projection"_s, matrix(input.scene.projection) },
             { u"fieldOfView"_s, input.scene.fieldOfView },
             { u"near"_s, input.scene.nearPlane },
             { u"far"_s, input.scene.farPlane },
             { u"entityIds"_s, ids },
             { u"models"_s, models },
             { u"focus"_s, input.focus },
             { u"aperture"_s, input.aperture } };
}
QSize sourceTarget(QSize logical, double dpr)
{
    return logical * dpr;
}
QList<QSize> sourceSizes()
{
    QList<QSize> sizes;
    for (const char *name :
         { "visual.json", "visual-dpr-1.json", "visual-dpr-1.5.json", "visual-dpr-2.json" }) {
        QFile file((QString::fromUtf8(FIXTURE_ROOT)) + "/tests/fixtures/postprocess/"_L1
                   + QString::fromUtf8(name));
        require(file.open(QIODevice::ReadOnly), "Missing postprocess source size input");
        for (const auto &value :
             QJsonDocument::fromJson(file.readAll()).object()[u"cases"_s].toArray()) {
            const auto row = value.toObject();
            if (row[u"group"_s] != u"size-aa"_s)
                continue;
            const QSize size(row[u"width"_s].toInt(), row[u"height"_s].toInt());
            if (!sizes.contains(size))
                sizes.append(size);
        }
    }
    require(sizes.size() == 9, "Postprocess input must contain all nine source sizes");
    return sizes;
}
void waitForCovers(RhiCaptureHarness &driver, SceneItem *item)
{
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool ready = false;
    const auto inspect = [&] {
        const auto stats = item->coverStatistics();
        ready = stats[u"identities"_s].toInt() > 0 && stats[u"ready"_s] == stats[u"identities"_s]
                && stats[u"selectedDetailed"_s].toBool();
        if (ready)
            loop.quit();
    };
    driver.surface->connectFrames(&loop, inspect);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    inspect();
    if (!ready) {
        timeout.start(yalahoo::review::DefaultDeadlineMilliseconds);
        driver.surface->requestFrame();
        loop.exec();
    }
    require(ready, "Anonymous production covers did not become ready");
}
void checkCoverFiltering()
{
    RhiCaptureHarness driver;
    SceneAdapter adapter;
    adapter.layout()->setViewport(193, 129);
    adapter.snapToTarget();
    Input input;
    input.scene = adapter.frame();
    input.entities = { input.scene.caseTransform };
    input.entityIds = { 1 };
    input.shadows = false;
    input.lightingEnabled = false;
    input.rawOutput = true;
    input.filterCovers = true;
    input.coverPainted = true;
    input.captureSamples = 4;
    input.atlasSize = { 512, 256 };
    input.atlasGeneration = 1;
    input.coverTiles = { QRect(0, 0, 256, 256) };
    input.coverLeases = { 1 };
    for (int slot = 0; slot < 2; ++slot) {
        QImage image(256, 256, QImage::Format_RGBA8888_Premultiplied);
        image.fill(slot ? Qt::blue : Qt::red);
        if (!slot)
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x)
                    if (x < 32 || x >= 224 || y < 32 || y >= 224)
                        image.setPixelColor(x, y, Qt::transparent);
        input.atlasUploads.append({ 1, 1, 1, slot, image, true, CoverCache::makeMipmaps(image) });
    }
    const auto result = driver.capture(input, { 193, 129 });
    require(result.metadata[u"sampling"_s].toObject()[u"coverTileLocalMipmaps"_s].toBool(),
            "Production cover filtering is not bound to the mipmapped texture");
    int redPixels = 0;
    for (int y = 0; y < result.color.height(); ++y)
        for (int x = 0; x < result.color.width(); ++x) {
            const auto color = result.color.pixelColor(x, y);
            if (color.redF() > .5f) {
                require(color.blueF() < .005f, "Cover mip filtering sampled a neighboring tile");
                ++redPixels;
            }
        }
    require(redPixels > 16, "Cover filtering probe did not draw the paper");
    input.atlasUploads.clear();
    const auto stable = driver.capture(input, { 193, 129 });
    require(stable.metadata[u"atlasUploadBytes"_s].toInteger() == 0,
            "Stable cover filtering regenerated or reuploaded mip levels");
}
QJsonObject captureState(QSize logical, double requestedDpr, const QString &output, int index)
{
    MotionController motion;
    SelectionController selection(&motion);
    SceneAdapter adapter(motion.clock());
    auto snapshot = productionLibrary();
    selection.setColumns(orderLibrary(*snapshot, SortMode::Genre).columns, snapshot);
    adapter.setSelection(&selection);
    adapter.layout()->setViewport(logical.width(), logical.height());
    adapter.snapToTarget();
    auto *quality = adapter.quality();
    RhiCaptureHarness driver([](QQuickItem *parent) {
        return new SceneItem(parent);
    });
    driver.manageGeometry = false;
    driver.window->resize(logical);
    if (OffscreenSurface::Enabled)
        driver.surface->setDevicePixelRatio(requestedDpr);
    auto *item = static_cast<SceneItem *>(driver.item);
    item->setSize(logical);
    item->setAdapter(&adapter);
    driver.replay(2);
    waitForCovers(driver, item);
    const double dpr = driver.window->effectiveDevicePixelRatio();
    require(!OffscreenSurface::Enabled || dpr == requestedDpr,
            "Actual target DPR differs from request");
    const auto expected = sourceTarget(logical, dpr);
    const QSize displayPixels = logical * dpr;
    QJsonArray themes;
    qint64 generation = -1;
    for (bool night : { false, true }) {
        adapter.lighting()->setNight(night);
        motion.clock()->setActive(true);
        for (int frame = 0; frame < 45; ++frame)
            motion.clock()->advance(1.0 / 60);
        driver.replay(2);
        require(quality->targetSize() == expected
                        && QSize(item->fixedColorBufferWidth(), item->fixedColorBufferHeight())
                                == expected,
                "SceneItem output differs from current native physical dimensions");
        const auto input = item->input();
        const auto capture = driver.capture(input, expected);
        checkPostprocessGraph(input, capture);
        require(item->errorText().isEmpty() && input.bokeh && input.aoSamples == 64 && input.smaa
                        && input.captureSamples == 4,
                "Scale audit changed authored effects or failed rendering");
        const auto bokeh = capture.metadata[u"submittedBokeh"_s].toObject();
        require(bokeh[u"width"_s].toInt() == expected.width()
                        && bokeh[u"height"_s].toInt() == expected.height()
                        && bokeh[u"depthWidth"_s].toInt() == expected.width()
                        && bokeh[u"depthHeight"_s].toInt() == expected.height()
                        && bokeh[u"focus"_s].toDouble() == input.focus
                        && bokeh[u"aperture"_s].toDouble() == input.aperture
                        && bokeh[u"maxblur"_s].toDouble() == double(.011f)
                        && bokeh[u"aspect"_s].toDouble()
                                == float(expected.width()) / expected.height(),
                "Bokeh uses logical pixels or stale full-resolution depth");
        const auto sampling = capture.metadata[u"sampling"_s].toObject();
        require(sampling[u"coverTileLocalMipmaps"_s].toBool()
                        && sampling[u"captureTrilinearClamp"_s].toBool(),
                "Cover or transmission filtering differs from source");
        const auto optical = capture.metadata[u"opticalGraph"_s].toObject();
        require(optical[u"captureMips"_s].toBool() && optical[u"captureBindingMatches"_s].toBool(),
                "Transmission capture lost its own mip chain or sampler binding");
        require(capture.metadata[u"visibleEntities"_s].toInt() >= 8,
                "Scale audit did not render the anonymous overlapping array");
        const auto resources = capture.metadata[u"resources"_s].toObject();
        QSet<QString> found;
        for (const auto &value : resources[u"entries"_s].toArray()) {
            const auto entry = value.toObject();
            const auto name = entry[u"name"_s].toString();
            if (name.startsWith(u"opaque-"_s) || name.startsWith(u"shell-composite-"_s)
                || name.startsWith(u"qt-item-"_s) || name.startsWith(u"dof-geometry-"_s)) {
                require(QSize(entry[u"width"_s].toInt(), entry[u"height"_s].toInt()) == expected,
                        "Physical geometry/output attachment has an unintended scale");
                found.insert(name);
            }
            if (name == "cover-or-atlas"_L1)
                require(entry[u"mipLevels"_s].toInt() > 1,
                        "Atlas is missing independent cover mip levels");
        }
        require(found.contains(u"opaque-color-resolve"_s) && found.contains(u"qt-item-color"_s),
                "Actual target inventory is missing");
        const auto finalImage = driver.surface->grab();
        require(finalImage.size() == displayPixels, "Final compositor ignored actual target DPR");
        const QString filename = u"/state-%1-%2.png"_s.arg(index).arg(night);
        require(finalImage.save(output + filename), "Cannot save own final compositor output");
        const auto actualGeneration = capture.metadata[u"resourceGeneration"_s].toInteger();
        require(generation < 0 || actualGeneration == generation,
                "Theme rebuilt compatible targets");
        generation = actualGeneration;
        themes.append(QJsonObject{ { u"night"_s, night },
                                   { u"image"_s, filename.mid(1) },
                                   { u"sourceState"_s, sourceState(input) },
                                   { u"metadata"_s, capture.metadata },
                                   { u"covers"_s, item->coverStatistics() } });
    }
    std::fprintf(stderr, "Native %dx%d DPR %.2f -> %dx%d passed\n", logical.width(),
                 logical.height(), dpr, expected.width(), expected.height());
    return { { u"logical"_s, pair(logical) },
             { u"requestedDpr"_s, requestedDpr },
             { u"actualDpr"_s, dpr },
             { u"sourceStageScale"_s, 1 },
             { u"physicalTarget"_s, pair(expected) },
             { u"displayPixels"_s, pair(displayPixels) },
             { u"finalScaleX"_s, double(displayPixels.width()) / expected.width() },
             { u"finalScaleY"_s, double(displayPixels.height()) / expected.height() },
             { u"offscreen"_s, driver.surface->diagnostics() },
             { u"windowPolicy"_s, verification::windowPolicy(*driver.window) },
             { u"themes"_s, themes } };
}
QJsonArray dprTransitions()
{
    QJsonArray states;
    if (!OffscreenSurface::Enabled)
        return states;
    MotionController motion;
    SelectionController selection(&motion);
    SceneAdapter adapter(motion.clock());
    auto snapshot = productionLibrary();
    selection.setColumns(orderLibrary(*snapshot, SortMode::Genre).columns, snapshot);
    adapter.setSelection(&selection);
    adapter.layout()->setViewport(640, 480);
    adapter.snapToTarget();
    RhiCaptureHarness driver([](QQuickItem *parent) {
        return new SceneItem(parent);
    });
    driver.manageGeometry = false;
    driver.window->resize(640, 480);
    auto *item = static_cast<SceneItem *>(driver.item);
    item->setSize({ 640, 480 });
    item->setAdapter(&adapter);
    driver.replay(2);
    waitForCovers(driver, item);
    quint64 atlasGeneration = item->input().atlasGeneration;
    QSize atlasSize = item->input().atlasSize;
    const auto originalState = sourceState(item->input());
    for (double dpr : { 1., 2., 1.5, 1. }) {
        driver.surface->setDevicePixelRatio(dpr);
        driver.replay(2);
        const auto expected = sourceTarget({ 640, 480 }, dpr);
        std::fprintf(
                stderr,
                "DPR transition requested=%.2f effective=%.2f quality=%dx%d fixed=%dx%d expected=%dx%d\n",
                dpr, driver.window->effectiveDevicePixelRatio(),
                adapter.quality()->targetSize().width(), adapter.quality()->targetSize().height(),
                item->fixedColorBufferWidth(), item->fixedColorBufferHeight(), expected.width(),
                expected.height());
        if (!item->errorText().isEmpty())
            std::fprintf(stderr, "Production scene error: %s\n",
                         item->errorText().toUtf8().constData());
        require(driver.window->effectiveDevicePixelRatio() == dpr
                        && adapter.quality()->targetSize() == expected
                        && QSize(item->fixedColorBufferWidth(), item->fixedColorBufferHeight())
                                == expected,
                "DPR change without resize failed to repolish the production target");
        const auto input = item->input();
        const auto result = driver.capture(input, expected);
        checkPostprocessGraph(input, result);
        require(sourceState(input) == originalState,
                "Same-window DPR transition changed source geometry, camera or optics");
        require((input.atlasSize == atlasSize ? input.atlasGeneration == atlasGeneration
                                              : input.atlasGeneration != atlasGeneration)
                        && result.metadata[u"atlasUploadBytes"_s].toInteger()
                                <= AtlasUploadQueue::steadyBudget
                        && driver.surface->grab().size() == QSize(640, 480) * dpr,
                "DPR change used a stale atlas generation or final compositor dimensions");
        atlasGeneration = input.atlasGeneration;
        atlasSize = input.atlasSize;
        states.append(QJsonObject{ { u"dpr"_s, dpr },
                                   { u"actualDpr"_s, driver.window->effectiveDevicePixelRatio() },
                                   { u"target"_s, pair(expected) },
                                   { u"metadata"_s, result.metadata } });
    }
    return states;
}

auto runScaleAudit(int &argc, char **&argv)
{
    const auto coverShutdown = qScopeGuard([] {
        yalahoo::CoverCache::finishDecoders();
    });
    if (argc != 2 && argc != 3)
        return 2;
    try {
        const QString output = QString::fromLocal8Bit(argv[1]);
        require(QDir().mkpath(output), "Cannot create scale output directory");
        require(argc == 2 || QByteArray(argv[2]) == "--representative"
                        || QByteArray(argv[2]) == "--filtering-only",
                "Unsupported native-size audit option");
        const auto coverSampling = checkLinearCoverSampling();
        checkCoverFiltering();
        const bool filteringOnly = argc == 3 && QByteArray(argv[2]) == "--filtering-only";
        const auto transitions = filteringOnly ? QJsonArray() : dprTransitions();
        QJsonArray states;
        const QList<double> dprs =
                OffscreenSurface::Enabled ? QList<double>{ 1, 1.5, 2 } : QList<double>{ 0 };
        const bool representative = argc == 3;
        const auto sizes =
                representative ? QList<QSize>{ { 321, 241 }, { 640, 480 } } : sourceSizes();
        for (auto size : filteringOnly ? QList<QSize>() : sizes) {
            QJsonArray sourceThemes;
            for (double dpr : dprs) {
                const auto state = captureState(size, dpr, output, states.size());
                const auto themes = state[u"themes"_s].toArray();
                if (!sourceThemes.isEmpty())
                    for (int i = 0; i < themes.size(); ++i)
                        require(themes[i].toObject()[u"sourceState"_s]
                                        == sourceThemes[i].toObject()[u"sourceState"_s],
                                "DPR changed source camera, motion, entity identities or optics");
                sourceThemes = themes;
                states.append(state);
            }
        }
        require(yalahoo::writeJsonFile(
                        output + "/runtime.json"_L1,
                        QJsonDocument(QJsonObject{
                                { u"round"_s, u"R19D4C"_s },
                                { u"passed"_s, true },
                                { u"states"_s, states },
                                { u"dprTransitions"_s, transitions },
                                { u"linearCoverSampling"_s, coverSampling },
                                { u"filteringOnly"_s, filteringOnly },
                                { u"nativeDprMatrixPending"_s, !OffscreenSurface::Enabled },
                                { u"scope"_s,
                                  u"Own RenderControl/native output, actual attachments and source math; no physical display/VSYNC or measured VRAM claim"_s } })),
                "Cannot commit scale report");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Scale audit: %s\n", error.what());
        return 1;
    }
}
}
int main(int argc, char **argv)
{
    return yalahoo::review::runApplication(argc, argv, [&](QGuiApplication &) {
        return runScaleAudit(argc, argv);
    });
}
