#include "RhiCaptureHarness.h"
#include "RhiProductionFixture.h"
#include "PostprocessChecks.h"
#include "GraphicsBackend.h"
#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSet>
#include <cmath>
#include <cstdio>
using namespace yalahoo;
using namespace yalahoo::rhi;
using namespace yalahoo::review;
namespace {
const auto check = RhiCaptureHarness::require;
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
    return { { "view", matrix(input.scene.view) },
             { "projection", matrix(input.scene.projection) },
             { "fieldOfView", input.scene.fieldOfView },
             { "near", input.scene.nearPlane },
             { "far", input.scene.farPlane },
             { "entityIds", ids },
             { "models", models },
             { "focus", input.focus },
             { "aperture", input.aperture } };
}
QSize sourceTarget(QSize logical, double dpr, const QVariantMap &values, int maximum)
{
    // Frozen renderDimensions with the production music.css host transform of one.
    const double requested =
            std::min(dpr, values["pixelRatio"].toDouble()) * values["scale"].toDouble() / 100;
    const double ratio = std::min(
            { requested, std::sqrt(8294400.0 / (double(logical.width()) * logical.height())),
              double(maximum) / std::max(logical.width(), logical.height()) });
    return { std::max(1, int(std::floor(logical.width() * ratio))),
             std::max(1, int(std::floor(logical.height() * ratio))) };
}
QList<QSize> sourceSizes()
{
    QList<QSize> sizes;
    for (const char *name :
         { "visual.json", "visual-dpr-1.json", "visual-dpr-1.5.json", "visual-dpr-2.json" }) {
        QFile file(QString(FIXTURE_ROOT) + "/tests/fixtures/postprocess/" + name);
        check(file.open(QIODevice::ReadOnly), "Missing postprocess source size input");
        for (const auto &value :
             QJsonDocument::fromJson(file.readAll()).object()["cases"].toArray()) {
            const auto row = value.toObject();
            if (row["group"] != "size-aa")
                continue;
            const QSize size(row["width"].toInt(), row["height"].toInt());
            if (!sizes.contains(size))
                sizes.append(size);
        }
    }
    check(sizes.size() == 9, "Postprocess input must contain all nine source sizes");
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
        ready = stats["identities"].toInt() > 0 && stats["ready"] == stats["identities"]
                && stats["selectedDetailed"].toBool();
        if (ready)
            loop.quit();
    };
    driver.surface.connectFrames(&loop, inspect);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    inspect();
    if (!ready) {
        timeout.start(60000);
        driver.surface.requestFrame();
        loop.exec();
    }
    check(ready, "Anonymous production covers did not become ready");
}
QJsonObject captureState(QSize logical, double requestedDpr, int scale, double aoResolution,
                         bool high, const QString &output, int index)
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
    quality->setPreset(high ? "high" : "original");
    auto values = quality->values();
    values["scale"] = scale;
    values["aoResolution"] = aoResolution;
    quality->configure(values);
    RhiCaptureHarness driver([](QQuickItem *parent) {
        return new SceneItem(parent);
    });
    driver.manageGeometry = false;
    driver.window.resize(logical);
    if (OffscreenSurface::Enabled)
        driver.surface.setDevicePixelRatio(requestedDpr);
    auto *item = static_cast<SceneItem *>(driver.item);
    item->setSize(logical);
    item->setAdapter(&adapter);
    driver.replay(2);
    waitForCovers(driver, item);
    const double dpr = driver.window.effectiveDevicePixelRatio();
    check(!OffscreenSurface::Enabled || dpr == requestedDpr,
          "Actual target DPR differs from request");
    const int maximum = driver.surface.rhi()->resourceLimit(QRhi::TextureSizeMax);
    const auto expected = sourceTarget(logical, dpr, values, maximum);
    const QSize displayPixels = logical * dpr;
    QJsonArray themes;
    qint64 generation = -1;
    for (bool night : { false, true }) {
        adapter.lighting()->setNight(night);
        motion.clock()->setActive(true);
        for (int frame = 0; frame < 45; ++frame)
            motion.clock()->advance(1.0 / 60);
        driver.replay(2);
        check(quality->targetSize() == expected
                      && QSize(item->fixedColorBufferWidth(), item->fixedColorBufferHeight())
                              == expected,
              "SceneItem applied extra stage scale or omitted the source pixel limit");
        const auto input = item->input();
        const auto capture = driver.capture(input, expected);
        checkPostprocessGraph(input, capture);
        check(item->errorText().isEmpty() && input.bokeh && input.aoSamples == 32
                      && input.smaa == high && input.captureSamples == 4,
              "Scale audit changed authored effects or failed rendering");
        const auto bokeh = capture.metadata["submittedBokeh"].toObject();
        check(bokeh["width"].toInt() == expected.width()
                      && bokeh["height"].toInt() == expected.height()
                      && bokeh["depthWidth"].toInt() == expected.width()
                      && bokeh["depthHeight"].toInt() == expected.height()
                      && bokeh["focus"].toDouble() == input.focus
                      && bokeh["aperture"].toDouble() == input.aperture
                      && bokeh["maxblur"].toDouble() == double(.011f)
                      && bokeh["aspect"].toDouble() == float(expected.width()) / expected.height(),
              "Bokeh uses logical pixels or stale full-resolution depth");
        const auto sampling = capture.metadata["sampling"].toObject();
        check(sampling["coverLinearClampNoMip"].toBool()
                      && sampling["captureTrilinearClamp"].toBool(),
              "Cover or transmission filtering differs from source");
        const auto optical = capture.metadata["opticalGraph"].toObject();
        check(optical["captureMips"].toBool() && optical["captureBindingMatches"].toBool(),
              "Transmission capture lost its own mip chain or sampler binding");
        check(capture.metadata["visibleEntities"].toInt() >= 8,
              "Scale audit did not render the anonymous overlapping array");
        const auto resources = capture.metadata["resources"].toObject();
        QSet<QString> found;
        for (const auto &value : resources["entries"].toArray()) {
            const auto entry = value.toObject();
            const auto name = entry["name"].toString();
            if (name.startsWith("opaque-") || name.startsWith("shell-composite-")
                || name.startsWith("qt-item-") || name.startsWith("dof-geometry-")) {
                check(QSize(entry["width"].toInt(), entry["height"].toInt()) == expected,
                      "Physical geometry/output attachment has an unintended scale");
                found.insert(name);
            }
            if (name == "cover-or-atlas")
                check(entry["mipLevels"].toInt() == 1, "Atlas created cross-tile mip levels");
        }
        check(found.contains("opaque-color-resolve") && found.contains("qt-item-color"),
              "Actual target inventory is missing");
        const auto finalImage = driver.surface.grab();
        check(finalImage.size() == displayPixels, "Final compositor ignored actual target DPR");
        const QString filename = QString("/state-%1-%2.png").arg(index).arg(night);
        check(finalImage.save(output + filename), "Cannot save own final compositor output");
        const auto actualGeneration = capture.metadata["resourceGeneration"].toInteger();
        check(generation < 0 || actualGeneration == generation, "Theme rebuilt compatible targets");
        generation = actualGeneration;
        themes.append(QJsonObject{ { "night", night },
                                   { "image", filename.mid(1) },
                                   { "sourceState", sourceState(input) },
                                   { "metadata", capture.metadata },
                                   { "covers", item->coverStatistics() } });
    }
    std::fprintf(stderr, "Scale %dx%d DPR %.2f scale %d -> %dx%d passed\n", logical.width(),
                 logical.height(), dpr, scale, expected.width(), expected.height());
    return { { "logical", pair(logical) },
             { "requestedDpr", requestedDpr },
             { "actualDpr", dpr },
             { "renderScale", scale },
             { "sourceStageScale", 1 },
             { "physicalTarget", pair(expected) },
             { "displayPixels", pair(displayPixels) },
             { "finalScaleX", double(displayPixels.width()) / expected.width() },
             { "finalScaleY", double(displayPixels.height()) / expected.height() },
             { "limited", quality->limited() },
             { "offscreen", driver.surface.diagnostics() },
             { "windowPolicy", verification::windowPolicy(driver.window) },
             { "themes", themes } };
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
    driver.window.resize(640, 480);
    auto *item = static_cast<SceneItem *>(driver.item);
    item->setSize({ 640, 480 });
    item->setAdapter(&adapter);
    driver.replay(2);
    waitForCovers(driver, item);
    const quint64 atlasGeneration = item->input().atlasGeneration;
    const auto originalState = sourceState(item->input());
    for (double dpr : { 1., 2., 1.5, 1. }) {
        driver.surface.setDevicePixelRatio(dpr);
        driver.replay(2);
        const auto expected =
                sourceTarget({ 640, 480 }, dpr, adapter.quality()->values(),
                             driver.surface.rhi()->resourceLimit(QRhi::TextureSizeMax));
        std::fprintf(
                stderr,
                "DPR transition requested=%.2f effective=%.2f quality=%dx%d fixed=%dx%d expected=%dx%d\n",
                dpr, driver.window.effectiveDevicePixelRatio(),
                adapter.quality()->targetSize().width(), adapter.quality()->targetSize().height(),
                item->fixedColorBufferWidth(), item->fixedColorBufferHeight(), expected.width(),
                expected.height());
        check(driver.window.effectiveDevicePixelRatio() == dpr
                      && adapter.quality()->targetSize() == expected
                      && QSize(item->fixedColorBufferWidth(), item->fixedColorBufferHeight())
                              == expected,
              "DPR change without resize failed to repolish the production target");
        const auto input = item->input();
        const auto result = driver.capture(input, expected);
        checkPostprocessGraph(input, result);
        check(sourceState(input) == originalState,
              "Same-window DPR transition changed source geometry, camera or optics");
        check(input.atlasGeneration == atlasGeneration
                      && result.metadata["atlasUploadBytes"].toInteger() == 0
                      && driver.surface.grab().size() == QSize(640, 480) * dpr,
              "DPR change retransmitted the atlas or used stale final compositor dimensions");
        states.append(QJsonObject{ { "dpr", dpr },
                                   { "actualDpr", driver.window.effectiveDevicePixelRatio() },
                                   { "target", pair(expected) },
                                   { "metadata", result.metadata } });
    }
    return states;
}
}
int main(int argc, char **argv)
{
    verification::prepareApplication();
    if (!selectGraphicsBackend())
        return 2;
    QGuiApplication app(argc, argv);
    if (argc != 2)
        return 2;
    try {
        const QString output = QString::fromLocal8Bit(argv[1]);
        check(QDir().mkpath(output), "Cannot create scale output directory");
        const auto transitions = dprTransitions();
        QJsonArray states;
        const QList<double> dprs =
                OffscreenSurface::Enabled ? QList<double>{ 1, 1.5, 2 } : QList<double>{ 0 };
        for (auto size : sourceSizes()) {
            QJsonArray sourceThemes;
            for (double dpr : dprs) {
                const auto state = captureState(size, dpr, 100, 1, false, output, states.size());
                const auto themes = state["themes"].toArray();
                if (!sourceThemes.isEmpty())
                    for (int i = 0; i < themes.size(); ++i)
                        check(themes[i].toObject()["sourceState"]
                                      == sourceThemes[i].toObject()["sourceState"],
                              "DPR changed source camera, motion, entity identities or optics");
                sourceThemes = themes;
                states.append(state);
            }
        }
        for (int scale : { 50, 125, 200 })
            states.append(
                    captureState({ 960, 900 }, 2, scale, .5, scale == 125, output, states.size()));
        QSaveFile file(output + "/runtime.json");
        check(file.open(QIODevice::WriteOnly), "Cannot open scale report");
        file.write(
                QJsonDocument(
                        QJsonObject{
                                { "round", "R19D4C" },
                                { "passed", true },
                                { "imageComparison", false },
                                { "states", states },
                                { "dprTransitions", transitions },
                                { "nativeDprMatrixPending", !OffscreenSurface::Enabled },
                                { "scope",
                                  "Own RenderControl/native output, actual attachments and source math; no physical display/VSYNC or measured VRAM claim" } })
                        .toJson());
        check(file.commit(), "Cannot commit scale report");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Scale audit: %s\n", error.what());
        return 1;
    }
}
