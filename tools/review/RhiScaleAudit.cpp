#include "CoverCache.h"
#include <QScopeGuard>
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

using namespace Qt::StringLiterals;

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
QSize sourceTarget(QSize logical, double dpr, const QVariantMap &values, int maximum)
{
    // Frozen renderDimensions with the production music.css host transform of one.
    const double requested =
            std::min(dpr, values[u"pixelRatio"_s].toDouble()) * values[u"scale"_s].toDouble() / 100;
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
        QFile file((QString::fromUtf8(FIXTURE_ROOT)) + "/tests/fixtures/postprocess/"_L1
                   + QString::fromUtf8(name));
        check(file.open(QIODevice::ReadOnly), "Missing postprocess source size input");
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
        ready = stats[u"identities"_s].toInt() > 0 && stats[u"ready"_s] == stats[u"identities"_s]
                && stats[u"selectedDetailed"_s].toBool();
        if (ready)
            loop.quit();
    };
    driver.surface->connectFrames(&loop, inspect);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    inspect();
    if (!ready) {
        timeout.start(60000);
        driver.surface->requestFrame();
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
    quality->setPreset(high ? u"high"_s : u"original"_s);
    auto values = quality->values();
    values[u"scale"_s] = scale;
    values[u"aoResolution"_s] = aoResolution;
    quality->configure(values);
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
    check(!OffscreenSurface::Enabled || dpr == requestedDpr,
          "Actual target DPR differs from request");
    const int maximum = driver.surface->rhi()->resourceLimit(QRhi::TextureSizeMax);
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
        const auto bokeh = capture.metadata[u"submittedBokeh"_s].toObject();
        check(bokeh[u"width"_s].toInt() == expected.width()
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
        check(sampling[u"coverLinearClampNoMip"_s].toBool()
                      && sampling[u"captureTrilinearClamp"_s].toBool(),
              "Cover or transmission filtering differs from source");
        const auto optical = capture.metadata[u"opticalGraph"_s].toObject();
        check(optical[u"captureMips"_s].toBool() && optical[u"captureBindingMatches"_s].toBool(),
              "Transmission capture lost its own mip chain or sampler binding");
        check(capture.metadata[u"visibleEntities"_s].toInt() >= 8,
              "Scale audit did not render the anonymous overlapping array");
        const auto resources = capture.metadata[u"resources"_s].toObject();
        QSet<QString> found;
        for (const auto &value : resources[u"entries"_s].toArray()) {
            const auto entry = value.toObject();
            const auto name = entry[u"name"_s].toString();
            if (name.startsWith(u"opaque-"_s) || name.startsWith(u"shell-composite-"_s)
                || name.startsWith(u"qt-item-"_s) || name.startsWith(u"dof-geometry-"_s)) {
                check(QSize(entry[u"width"_s].toInt(), entry[u"height"_s].toInt()) == expected,
                      "Physical geometry/output attachment has an unintended scale");
                found.insert(name);
            }
            if (name == "cover-or-atlas"_L1)
                check(entry[u"mipLevels"_s].toInt() == 1, "Atlas created cross-tile mip levels");
        }
        check(found.contains(u"opaque-color-resolve"_s) && found.contains(u"qt-item-color"_s),
              "Actual target inventory is missing");
        const auto finalImage = driver.surface->grab();
        check(finalImage.size() == displayPixels, "Final compositor ignored actual target DPR");
        const QString filename = u"/state-%1-%2.png"_s.arg(index).arg(night);
        check(finalImage.save(output + filename), "Cannot save own final compositor output");
        const auto actualGeneration = capture.metadata[u"resourceGeneration"_s].toInteger();
        check(generation < 0 || actualGeneration == generation, "Theme rebuilt compatible targets");
        generation = actualGeneration;
        themes.append(QJsonObject{ { u"night"_s, night },
                                   { u"image"_s, filename.mid(1) },
                                   { u"sourceState"_s, sourceState(input) },
                                   { u"metadata"_s, capture.metadata },
                                   { u"covers"_s, item->coverStatistics() } });
    }
    std::fprintf(stderr, "Scale %dx%d DPR %.2f scale %d -> %dx%d passed\n", logical.width(),
                 logical.height(), dpr, scale, expected.width(), expected.height());
    return { { u"logical"_s, pair(logical) },
             { u"requestedDpr"_s, requestedDpr },
             { u"actualDpr"_s, dpr },
             { u"renderScale"_s, scale },
             { u"sourceStageScale"_s, 1 },
             { u"physicalTarget"_s, pair(expected) },
             { u"displayPixels"_s, pair(displayPixels) },
             { u"finalScaleX"_s, double(displayPixels.width()) / expected.width() },
             { u"finalScaleY"_s, double(displayPixels.height()) / expected.height() },
             { u"limited"_s, quality->limited() },
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
    const quint64 atlasGeneration = item->input().atlasGeneration;
    const auto originalState = sourceState(item->input());
    for (double dpr : { 1., 2., 1.5, 1. }) {
        driver.surface->setDevicePixelRatio(dpr);
        driver.replay(2);
        const auto expected =
                sourceTarget({ 640, 480 }, dpr, adapter.quality()->values(),
                             driver.surface->rhi()->resourceLimit(QRhi::TextureSizeMax));
        std::fprintf(
                stderr,
                "DPR transition requested=%.2f effective=%.2f quality=%dx%d fixed=%dx%d expected=%dx%d\n",
                dpr, driver.window->effectiveDevicePixelRatio(),
                adapter.quality()->targetSize().width(), adapter.quality()->targetSize().height(),
                item->fixedColorBufferWidth(), item->fixedColorBufferHeight(), expected.width(),
                expected.height());
        check(driver.window->effectiveDevicePixelRatio() == dpr
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
                      && result.metadata[u"atlasUploadBytes"_s].toInteger() == 0
                      && driver.surface->grab().size() == QSize(640, 480) * dpr,
              "DPR change retransmitted the atlas or used stale final compositor dimensions");
        states.append(QJsonObject{ { u"dpr"_s, dpr },
                                   { u"actualDpr"_s, driver.window->effectiveDevicePixelRatio() },
                                   { u"target"_s, pair(expected) },
                                   { u"metadata"_s, result.metadata } });
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
    if (!yalahoo::probeOptionalGraphicsBackend())
        return 22;
    const auto coverShutdown = qScopeGuard([] {
        yalahoo::CoverCache::finishDecoders();
    });
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
                const auto themes = state[u"themes"_s].toArray();
                if (!sourceThemes.isEmpty())
                    for (int i = 0; i < themes.size(); ++i)
                        check(themes[i].toObject()[u"sourceState"_s]
                                      == sourceThemes[i].toObject()[u"sourceState"_s],
                              "DPR changed source camera, motion, entity identities or optics");
                sourceThemes = themes;
                states.append(state);
            }
        }
        for (int scale : { 50, 125, 200 })
            states.append(
                    captureState({ 960, 900 }, 2, scale, .5, scale == 125, output, states.size()));
        QSaveFile file(output + "/runtime.json"_L1);
        check(file.open(QIODevice::WriteOnly), "Cannot open scale report");
        file.write(
                QJsonDocument(
                        QJsonObject{
                                { u"round"_s, u"R19D4C"_s },
                                { u"passed"_s, true },
                                { u"states"_s, states },
                                { u"dprTransitions"_s, transitions },
                                { u"nativeDprMatrixPending"_s, !OffscreenSurface::Enabled },
                                { u"scope"_s,
                                  u"Own RenderControl/native output, actual attachments and source math; no physical display/VSYNC or measured VRAM claim"_s } })
                        .toJson());
        check(file.commit(), "Cannot commit scale report");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Scale audit: %s\n", error.what());
        return 1;
    }
}
