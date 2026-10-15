#include "RhiCaptureHarness.h"
#include "RhiReviewInput.h"
#include "RhiFogAudit.h"
#include "SceneItem.h"
#include "OverviewController.h"
#include "PresentationController.h"
#include "GraphicsBackend.h"
#include <QGuiApplication>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QDir>
#include <cstdio>
using namespace yalahoo;
using namespace yalahoo::rhi;
using namespace yalahoo::review;
namespace {
void check(bool condition, const char *message)
{
    RhiCaptureHarness::require(condition, message);
}
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
    return { { "missing", missing }, { "shaded", shaded }, { "nonfinite", nonfinite } };
}
Snapshot library()
{
    LibrarySnapshot result;
    for (int lane = 0; lane < 9; ++lane) {
        Genre genre;
        genre.id = QString::number(lane);
        genre.name = genre.id;
        result.genres.append(genre);
        for (int row = 0; row < 64; ++row) {
            Album album;
            album.id = QString("%1:%2").arg(lane).arg(row);
            album.genreId = QString::number(lane);
            album.title = QString("Anonymous album %1").arg(row);
            album.coverUrl =
                    QUrl(QString("qrc:/assets/demo-covers/%1.png")
                                 .arg(QStringList{ "square", "portrait", "landscape" }[row % 3]));
            result.albums.append(album);
        }
    }
    return std::make_shared<const LibrarySnapshot>(std::move(result));
}
}
int main(int argc, char **argv)
{
    verification::prepareApplication();
    if (!selectGraphicsBackend())
        return 2;
    QGuiApplication app(argc, argv);
    const bool opticsOnly = argc == 3 && QString::fromLocal8Bit(argv[2]) == "--optics-only";
    if (argc != 2 && !opticsOnly)
        return 2;
    if (qEnvironmentVariableIntValue("YALAHOO_VERIFY_VISIBLE") == 1)
        OffscreenSurface::Enabled = false;
    try {
        const QString output = QString::fromLocal8Bit(argv[1]);
        check(QDir().mkpath(output), "Cannot create diagnostic output");
        QFile file(QString(FIXTURE_ROOT) + "/tests/fixtures/rhi-shell/visual.json");
        check(file.open(QIODevice::ReadOnly), "Missing source input");
        auto cases = QJsonDocument::fromJson(file.readAll()).object()["cases"].toArray();
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
                    sample["entities"] = count;
                    sample["samples"] = samples;
                    isolated.append(sample);
                    std::fprintf(
                            stderr,
                            "Visibility entities=%d samples=%d missing=%d shaded=%d nonfinite=%d\n",
                            count, samples, sample["missing"].toInt(), sample["shaded"].toInt(),
                            sample["nonfinite"].toInt());
                    result.composite.save(output
                                          + QString("/isolated-%1-%2.png").arg(count).arg(samples));
                    check(sample["missing"].toInt() == 0 && sample["shaded"].toInt() > 100
                                  && sample["nonfinite"].toInt() == 0,
                          "Visibility fragments lost at Equal shading or nonfinite optics");
                }
            }
        }
        QJsonArray sequences;
        for (QSize size : { QSize(640, 480), QSize(1280, 720) }) {
            MotionController motion;
            OverviewController overview(&motion);
            SelectionController selection(&motion);
            SceneAdapter adapter(motion.clock());
            PresentationController presentation(motion.clock(), { });
            auto snapshot = library();
            selection.setColumns(orderLibrary(*snapshot, SortMode::Genre).columns, snapshot);
            adapter.setSelection(&selection);
            overview.bind(&adapter, &selection, &presentation);
            adapter.layout()->setViewport(size.width(), size.height());
            adapter.snapToTarget();
            RhiCaptureHarness driver([](QQuickItem *parent) {
                return new SceneItem(parent);
            });
            driver.manageGeometry = false;
            driver.window.resize(size);
            auto *item = static_cast<SceneItem *>(driver.item);
            item->setSize(size);
            item->setAdapter(&adapter);
            item->setProperty("_yalahooReviewOpticsOnly", opticsOnly);
            driver.replay(1);
            {
                QEventLoop wait;
                QTimer timeout;
                timeout.setSingleShot(true);
                bool ready = false;
                const auto checkReady = [&] {
                    const auto stats = item->coverStatistics();
                    ready = stats["identities"].toInt() > 0
                            && stats["identities"].toInt() == stats["ready"].toInt();
                    if (ready)
                        wait.quit();
                };
                driver.surface.connectFrames(&wait, checkReady);
                QObject::connect(&timeout, &QTimer::timeout, &wait, &QEventLoop::quit);
                checkReady();
                if (!ready) {
                    timeout.start(60000);
                    driver.surface.requestFrame();
                    wait.exec();
                }
                if (!ready)
                    std::fprintf(stderr, "%s\n",
                                 QJsonDocument(item->coverStatistics()).toJson().constData());
                check(ready, "Production covers not ready");
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
                        auto input = item->input();
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
                        checkSubmittedFog(input, result);
                        check(input.fogEnabled, "Production scene did not enable authored fog");
                        const double distance =
                                (input.scene.camera.position - input.scene.camera.aim).length();
                        check(std::abs(input.scene.fogNear
                                       - (distance + 5 * (1 - adapter.detail()) - adapter.detail()))
                                              < 1e-10
                                      && std::abs(input.scene.fogFar
                                                  - (distance + 25 * (1 - adapter.detail())
                                                     + 12 * adapter.detail()))
                                              < 1e-10,
                              "Production fog is not anchored to the rendered camera");
                        check(item->errorText().isEmpty(), "Production renderer failed");
                        check(result.metadata.contains("opticalProbeSample")
                                      == (input.opticalProbe != 0),
                              "Capture used the wrong optical mode");
                        const auto actualGeneration =
                                result.metadata["resourceGeneration"].toInteger();
                        if (generation >= 0)
                            check(generation == actualGeneration,
                                  "Frame/probe change rebuilt render targets");
                        generation = actualGeneration;
                        check(result.metadata["visibleEntities"].toInt() >= 8,
                              "Missing batched production overlap");
                        check(result.metadata["atlasUploadBytes"].toInteger()
                                      <= AtlasUploadQueue::steadyBudget,
                              "Unbounded cover uploads");
                        auto sample = input.opticalProbe ? coverage(result) : QJsonObject{ };
                        if (input.opticalProbe) {
                            if (sample["missing"].toInt() || sample["nonfinite"].toInt()
                                || sample["shaded"].toInt() <= 100) {
                                std::fprintf(
                                        stderr,
                                        "Production failure %dx%d night=%d stage=%d frame=%d %s\n",
                                        size.width(), size.height(), night, stage, frame,
                                        QJsonDocument(sample)
                                                .toJson(QJsonDocument::Compact)
                                                .constData());
                                result.composite.save(output + "/failed-visibility.png");
                                QSaveFile failed(output + "/failed-metadata.json");
                                if (failed.open(QIODevice::WriteOnly)) {
                                    failed.write(QJsonDocument(result.metadata).toJson());
                                    failed.commit();
                                }
                            }
                            check(sample["missing"].toInt() == 0 && sample["nonfinite"].toInt() == 0
                                          && sample["shaded"].toInt() > 100,
                                  "Production overlap has Equal holes or nonfinite optics");
                        }
                        sample["frame"] = frame;
                        sample["diagnostic"] = input.opticalProbe != 0;
                        sample["width"] = target.width();
                        sample["height"] = target.height();
                        sample["sceneRevision"] = qint64(item->input().scene.revision);
                        sample["generation"] = result.metadata["resourceGeneration"];
                        sample["visibleEntities"] = result.metadata["visibleEntities"];
                        sample["shells"] = result.metadata["sortedShells"].toArray().size();
                        sample["overview"] = overview.progress();
                        sample["detail"] = adapter.detail();
                        sample["fogNear"] = input.scene.fogNear;
                        sample["fogFar"] = input.scene.fogFar;
                        sample["fogColor"] = QJsonArray{ input.background.x(), input.background.y(),
                                                         input.background.z() };
                        frames.append(sample);
                        if (frame % 15 == 1 && input.opticalProbe == 0)
                            check((opticsOnly ? result.composite : result.postprocessed)
                                          .save(output
                                                + QString("/production-%1-%2-%3-%4.png")
                                                          .arg(size.width())
                                                          .arg(night)
                                                          .arg(stage)
                                                          .arg(frame)),
                                  "Cannot save own continuous output");
                    }
                    std::fprintf(stderr, "Production %dx%d night=%d stage=%d passed 60 frames\n",
                                 size.width(), size.height(), night, stage);
                    sequences.append(QJsonObject{
                            { "width", size.width() },
                            { "height", size.height() },
                            { "night", night },
                            { "stage", stage },
                            { "devicePixelRatio", driver.window.devicePixelRatio() },
                            { "offscreen", driver.surface.diagnostics() },
                            { "windowPolicy", verification::windowPolicy(driver.window) },
                            { "frames", frames } });
                }
            }
            item->captured = { };
        }
        QSaveFile report(output + "/runtime.json");
        check(report.open(QIODevice::WriteOnly), "Cannot write report");
        report.write(QJsonDocument(QJsonObject{ { "round", "R19C2C" },
                                                { "passed", true },
                                                { "imageComparison", false },
                                                { "opticsOnly", opticsOnly },
                                                { "qt", qVersion() },
                                                { "isolated", isolated },
                                                { "sequences", sequences } })
                             .toJson());
        check(report.commit(), "Cannot commit report");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
