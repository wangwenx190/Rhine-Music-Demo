#include "Require.h"
#include "ReviewSupport.h"
#include "ReviewApplication.h"
#include "SceneAdapter.h"
#include "MotionController.h"
#include "SelectionController.h"
#include "CoverCache.h"
#include <QScopeGuard>
#include "RhiCaptureHarness.h"
#include "RhiReviewInput.h"
#include "RhiFogChecks.h"
#include "RhiShadowChecks.h"
#include "SceneItem.h"
#include "RhiProductionFixture.h"
#include "OverviewController.h"
#include "PresentationController.h"
#include "GraphicsBackend.h"
#include "PostprocessChecks.h"
#include <QGuiApplication>
#include <QFile>
#include <QJsonDocument>
#include "JsonFile.h"
#include <QDir>
#include <cstdio>

using namespace Qt::StringLiterals;

using namespace yalahoo;
using namespace yalahoo::rhi;
using namespace yalahoo::review;

namespace {
QJsonObject coverage(const Capture &capture)
{
    int missing = 0, shaded = 0, nonfinite = 0;
    const auto image = capture.composite.convertToFormat(QImage::Format_RGBA8888);
    for (int y = 0; y < image.height(); ++y) {
        const auto *line = image.constScanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            const auto *p = line + x * 4;
            missing += p[0] == 255 && p[1] < 255 && p[2] == 255;
            shaded += p[0] == 255 && p[1] == 255 && p[2] == 255;
            nonfinite += p[0] == 0 && p[1] == 255 && p[2] == 0;
        }
    }
    return { { u"missing"_s, missing }, { u"shaded"_s, shaded }, { u"nonfinite"_s, nonfinite } };
}

void writeFailureMetadata(const QString &output, const QJsonObject &metadata)
{
    const bool saved =
            yalahoo::writeJsonFile(output + "/failed-metadata.json"_L1, QJsonDocument(metadata));
    if (!saved)
        qWarning("Cannot save failure metadata");
}
auto runFlickerChecks(int &argc, char **&argv)
{
    const auto coverShutdown = qScopeGuard([] {
        yalahoo::CoverCache::finishDecoders();
    });
    const bool opticsOnly = argc == 3 && QString::fromLocal8Bit(argv[2]) == "--optics-only"_L1;
    const bool isolatedOnly = argc == 3 && QByteArray(argv[2]) == "--isolated-only"_ba;
    if (argc != 2 && !opticsOnly && !isolatedOnly)
        return 2;
    if (qEnvironmentVariableIntValue("YALAHOO_VERIFY_VISIBLE") == 1)
        OffscreenSurface::Enabled = false;
    try {
        const QString output = QString::fromLocal8Bit(argv[1]);
        require(QDir().mkpath(output), "Cannot create diagnostic output");
        QFile file((QString::fromUtf8(FIXTURE_ROOT)) + "/tests/fixtures/rhi-shell/visual.json"_L1);
        require(file.open(QIODevice::ReadOnly), "Missing source input");
        auto cases = QJsonDocument::fromJson(file.readAll()).object()[u"cases"_s].toArray();
        QJsonArray isolated;
        {
            RhiCaptureHarness driver;
            auto input = fixture(cases[0].toObject());
            input.opticalProbe = 10;
            input.shadows = false;
            for (int count : { 1, 7, 8, 9 }) {
                input.entities.clear();
                input.entityIds.clear();
                for (int i = 0; i < count; ++i) {
                    QMatrix4x4 m;
                    m.translate((i % 3 - 1) * 2.4f, (i / 3 - 1) * 2.4f, -.13f * i);
                    m.rotate(17.3f + i * .7f, 0, 1, 0);
                    input.entities.append(m);
                    input.entityIds.append(i + 1);
                }
                for (int samples : { 1, 4 }) {
                    input.captureSamples = samples;
                    auto result = driver.capture(input, { 960, 720 });
                    auto sample = coverage(result);
                    sample[u"entities"_s] = count;
                    sample[u"samples"_s] = samples;
                    isolated.append(sample);
                    std::fprintf(
                            stderr,
                            "Visibility entities=%d samples=%d missing=%d shaded=%d nonfinite=%d\n",
                            count, samples, sample[u"missing"_s].toInt(),
                            sample[u"shaded"_s].toInt(), sample[u"nonfinite"_s].toInt());
                    result.composite.save(output
                                          + u"/isolated-%1-%2.png"_s.arg(count).arg(samples));
                    require(sample[u"missing"_s].toInt() == 0 && sample[u"shaded"_s].toInt() > 100
                                    && sample[u"nonfinite"_s].toInt() == 0,
                            "Visibility fragments lost at Equal shading or nonfinite optics");
                }
            }
        }
        QJsonArray sequences;
        const QList<QSize> sizes =
                isolatedOnly ? QList<QSize>() : QList<QSize>{ { 640, 480 }, { 1280, 720 } };
        for (QSize size : sizes) {
            MotionController motion;
            OverviewController overview(&motion);
            SelectionController selection(&motion);
            SceneAdapter adapter(motion.clock());
            PresentationController presentation(motion.clock(), { });
            auto snapshot = productionLibrary();
            selection.setColumns(orderLibrary(*snapshot, SortMode::Genre).columns, snapshot);
            adapter.setSelection(&selection);
            overview.bind(&adapter, &selection, &presentation);
            adapter.layout()->setViewport(size.width(), size.height());
            adapter.snapToTarget();
            RhiCaptureHarness driver([](QQuickItem *parent) {
                return new SceneItem(parent);
            });
            driver.manageGeometry = false;
            driver.window->resize(size);
            auto *item = static_cast<SceneItem *>(driver.item);
            item->setSize(size);
            item->setAdapter(&adapter);
            item->setProperty("_yalahooReviewOpticsOnly", opticsOnly);
            item->ensurePolished();
            driver.replay(1);
            {
                QEventLoop wait;
                QTimer timeout;
                timeout.setSingleShot(true);
                bool ready = false;
                const auto checkReady = [&] {
                    const auto stats = item->coverStatistics();
                    ready = stats[u"identities"_s].toInt() > 0
                            && stats[u"identities"_s].toInt() == stats[u"ready"_s].toInt();
                    if (ready)
                        wait.quit();
                };
                driver.surface->connectFrames(&wait, checkReady);
                QObject::connect(&timeout, &QTimer::timeout, &wait, &QEventLoop::quit);
                checkReady();
                if (!ready) {
                    timeout.start(yalahoo::review::DefaultDeadlineMilliseconds);
                    driver.surface->requestFrame();
                    wait.exec();
                }
                if (!ready)
                    std::fprintf(stderr, "%s\n",
                                 QJsonDocument(item->coverStatistics()).toJson().constData());
                require(ready, "Production covers not ready");
            }
            motion.clock()->setActive(true);
            motion.clock()->advance(1.0 / 60);
            for (bool night : { false, true }) {
                adapter.lighting()->setNight(night);
                // Settle the authored theme transition without reducing scene motion.
                for (int i = 0; i < 45; ++i)
                    motion.clock()->advance(1.0 / 60);
                for (int stage = 0; stage < 5; ++stage) {
                    overview.setActive(false);
                    if (stage == 2)
                        selection.moveRow(1);
                    if (stage == 3)
                        overview.setActive(true);
                    QJsonArray frames;
                    qint64 generation = -1;
                    for (int frame = 0; frame < 60; ++frame) {
                        if (stage == 4)
                            adapter.setDetail(frame < 20 ? .5 : frame < 40 ? 1 : 0);
                        if (stage == 3 && frame == 30)
                            overview.setActive(false);
                        if (stage != 0)
                            motion.clock()->advance(1.0 / 60);
                        // The production SceneItem publishes the real source state/cover leases.
                        driver.replay(1);
                        // A queued native frameSwapped may describe the previous frame.
                        // Publish the current production snapshot before taking its oracle.
                        item->ensurePolished();
                        auto input = item->input();
                        require(input.scene.revision == adapter.frame().revision,
                                "Production snapshot is stale after polish");
                        input.opticalProbe = frame < 30 ? 10 : 0;
                        if (opticsOnly) {
                            input.postprocess = false;
                            input.aoSamples = 0;
                            input.bokeh = false;
                            input.smaa = false;
                        }
                        const QSize target(item->fixedColorBufferWidth(),
                                           item->fixedColorBufferHeight());
                        auto result = driver.capture(input, target);
                        checkPostprocessGraph(input, result);
                        checkSubmittedFog(input, result);
                        checkSubmittedShadow(input, result);
                        if (input.bokeh) {
                            const auto uploaded = result.metadata[u"submittedBokeh"_s].toObject();
                            const float focus =
                                    -(input.scene.view
                                      * QVector4D(adapter.casePosition() + QVector3D(0, 2, 0), 1))
                                             .z();
                            const float aperture = float(
                                    (.0003 + .0005 * adapter.detail())
                                    * adapter.quality()->values()[u"depthOfField"_s].toDouble()
                                    / 100);
                            const bool bokehMatches = uploaded[u"focus"_s].toDouble() == focus
                                    && uploaded[u"aperture"_s].toDouble() == aperture
                                    && uploaded[u"maxblur"_s].toDouble() == double(.011f)
                                    && uploaded[u"near"_s].toDouble()
                                            == float(input.scene.nearPlane)
                                    && uploaded[u"far"_s].toDouble() == float(input.scene.farPlane)
                                    && uploaded[u"aspect"_s].toDouble()
                                            == float(target.width()) / target.height()
                                    && uploaded[u"depthWidth"_s].toInt() == target.width()
                                    && uploaded[u"depthHeight"_s].toInt() == target.height();
                            if (!bokehMatches)
                                qWarning() << "Bokeh submission mismatch" << size << night << stage
                                           << frame << "focus" << focus << input.focus
                                           << uploaded[u"focus"_s] << "aperture" << aperture
                                           << input.aperture << uploaded[u"aperture"_s]
                                           << "uploaded" << uploaded;
                            require(bokehMatches,
                                    "Actual production Bokeh uniform/depth differs from frozen source");
                        }
                        const auto floor = result.metadata[u"productFloor"_s].toObject();
                        require(input.productFloor && floor[u"enabled"_s].toBool()
                                        && floor[u"opaqueIndices"_s].toInt() == 6
                                        && (opticsOnly
                                            || (floor[u"aoIndices"_s].toInt() == 6
                                                && floor[u"dofIndices"_s].toInt() == 6)),
                                "Production floor omitted from capture or post geometry");
                        for (int c = 0; c < 3; ++c)
                            require(floor[u"color"_s].toArray()[c].toDouble()
                                            == adapter.lighting()->palette().floor.qt()[c],
                                    "Production floor uses stale theme color");
                        require(input.fogEnabled, "Production scene did not enable authored fog");
                        const double distance =
                                (input.scene.camera.position - input.scene.camera.aim).length();
                        require(std::abs(input.scene.fogNear
                                         - (distance + 5 * (1 - adapter.detail())
                                            - adapter.detail()))
                                                < 1e-10
                                        && std::abs(input.scene.fogFar
                                                    - (distance + 25 * (1 - adapter.detail())
                                                       + 12 * adapter.detail()))
                                                < 1e-10,
                                "Production fog is not anchored to the rendered camera");
                        require(item->errorText().isEmpty(), "Production renderer failed");
                        require(result.metadata.contains(u"opticalProbeSample"_s)
                                        == (input.opticalProbe != 0),
                                "Capture used the wrong optical mode");
                        const auto actualGeneration =
                                result.metadata[u"resourceGeneration"_s].toInteger();
                        if (generation >= 0)
                            require(generation == actualGeneration,
                                    "Frame/probe change rebuilt render targets");
                        generation = actualGeneration;
                        require(result.metadata[u"visibleEntities"_s].toInt() >= 8,
                                "Missing batched production overlap");
                        require(result.metadata[u"atlasUploadBytes"_s].toInteger()
                                        <= AtlasUploadQueue::steadyBudget,
                                "Unbounded cover uploads");
                        auto sample = input.opticalProbe ? coverage(result) : QJsonObject{ };
                        if (input.opticalProbe) {
                            if (sample[u"missing"_s].toInt() || sample[u"nonfinite"_s].toInt()
                                || sample[u"shaded"_s].toInt() <= 100) {
                                std::fprintf(
                                        stderr,
                                        "Production failure %dx%d night=%d stage=%d frame=%d %s\n",
                                        size.width(), size.height(), night, stage, frame,
                                        QJsonDocument(sample)
                                                .toJson(QJsonDocument::Compact)
                                                .constData());
                                result.composite.save(output + "/failed-visibility.png"_L1);
                                writeFailureMetadata(output, result.metadata);
                            }
                            require(sample[u"missing"_s].toInt() == 0
                                            && sample[u"nonfinite"_s].toInt() == 0
                                            && sample[u"shaded"_s].toInt() > 100,
                                    "Production overlap has Equal holes or nonfinite optics");
                        }
                        sample[u"frame"_s] = frame;
                        sample[u"diagnostic"_s] = input.opticalProbe != 0;
                        sample[u"width"_s] = target.width();
                        sample[u"height"_s] = target.height();
                        sample[u"sceneRevision"_s] = qint64(item->input().scene.revision);
                        sample[u"generation"_s] = result.metadata[u"resourceGeneration"_s];
                        sample[u"visibleEntities"_s] = result.metadata[u"visibleEntities"_s];
                        sample[u"shells"_s] = result.metadata[u"sortedShells"_s].toArray().size();
                        sample[u"overview"_s] = overview.progress();
                        sample[u"detail"_s] = adapter.detail();
                        sample[u"fogNear"_s] = input.scene.fogNear;
                        sample[u"fogFar"_s] = input.scene.fogFar;
                        sample[u"fogColor"_s] =
                                QJsonArray{ input.background.x(), input.background.y(),
                                            input.background.z() };
                        frames.append(sample);
                        if (frame % 15 == 1 && input.opticalProbe == 0)
                            require((opticsOnly ? result.composite : driver.surface->grab())
                                            .save(output
                                                  + u"/production-%1-%2-%3-%4.png"_s
                                                            .arg(size.width())
                                                            .arg(night)
                                                            .arg(stage)
                                                            .arg(frame)),
                                    "Cannot save own continuous output");
                    }
                    std::fprintf(stderr, "Production %dx%d night=%d stage=%d passed 60 frames\n",
                                 size.width(), size.height(), night, stage);
                    sequences.append(QJsonObject{
                            { u"width"_s, size.width() },
                            { u"height"_s, size.height() },
                            { u"night"_s, night },
                            { u"stage"_s, stage },
                            { u"devicePixelRatio"_s, driver.window->devicePixelRatio() },
                            { u"offscreen"_s, driver.surface->diagnostics() },
                            { u"windowPolicy"_s, verification::windowPolicy(*driver.window) },
                            { u"frames"_s, frames } });
                }
            }
            item->captured = { };
        }
        require(yalahoo::writeJsonFile(
                        output + "/runtime.json"_L1,
                        QJsonDocument(QJsonObject{ { u"passed"_s, true },
                                                   { u"opticsOnly"_s, opticsOnly },
                                                   { u"isolatedOnly"_s, isolatedOnly },
                                                   { u"qt"_s, QString::fromUtf8(qVersion()) },
                                                   { u"isolated"_s, isolated },
                                                   { u"sequences"_s, sequences } })),
                "Cannot commit report");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
}
int main(int argc, char **argv)
{
    return yalahoo::review::runApplication(argc, argv, [&](QGuiApplication &) {
        return runFlickerChecks(argc, argv);
    });
}
