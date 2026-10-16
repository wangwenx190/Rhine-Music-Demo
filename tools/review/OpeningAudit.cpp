#include "OpeningAudit.h"
#include "OffscreenSurface.h"
#include "OpeningController.h"
#include "OpeningInk.h"
#include "SceneItem.h"
#include "LibrarySession.h"
#include "VerificationWindow.h"
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QEventLoop>
#include <QTimer>
#include <QKeyEvent>
#include <QSignalSpy>
#include <stdexcept>
using namespace yalahoo;
namespace {
void require(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}
QQuickItem *find(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name)
        return root;
    for (auto *child : root->childItems())
        if (auto *item = find(child, name))
            return item;
    return nullptr;
}
class Driver
{
public:
    Driver() : window(OffscreenSurface::Enabled ? &control : nullptr), surface(window, control)
    {
        verification::prepareWindow(window, true);
        window.setPersistentGraphics(false);
        window.setPersistentSceneGraph(false);
        window.resize(1280, 720);
        motion.clock()->setActive(true);
        motion.clock()->advance(0);
        LibrarySnapshot snapshot;
        Genre genre;
        genre.id = "opening-demo";
        genre.name = "匿名序章收藏";
        snapshot.genres.append(genre);
        for (int i = 0; i < 3; ++i) {
            Album album;
            album.id = QStringLiteral("opening-album-%1").arg(i);
            album.title = QStringLiteral("匿名专辑 %1").arg(i + 1);
            album.genreId = genre.id;
            album.coverUrl = QUrl("qrc:/assets/demo-covers/square.png");
            snapshot.albums.append(album);
        }
        library.setSnapshot(std::move(snapshot));
        selection.setColumns(library.albums()->order().columns, library.snapshot());
        scene.setSelection(&selection);
        QObject::connect(&engine, &QQmlEngine::warnings, &window,
                         [&](const QList<QQmlError> &errors) {
                             for (const auto &error : errors)
                                 warnings.append(error.toString());
                         });
        QQmlComponent component(&engine, QUrl("qrc:/qt/qml/Yalahoo/Music/pages/MusicStage.qml"));
        root.reset(qobject_cast<QQuickItem *>(component.createWithInitialProperties(
                { { "librarySession", QVariant::fromValue(&library) },
                  { "motionController", QVariant::fromValue(&motion) },
                  { "sceneAdapter", QVariant::fromValue(&scene) },
                  { "openingController", QVariant::fromValue(&opening) },
                  { "offscreenRendering", true } })));
        if (!root)
            qWarning() << component.errors();
        require(bool(root), "Cannot create production MusicStage");
        motion.clock()->setActive(true);
        motion.clock()->advance(0);
        root->setParentItem(window.contentItem());
        root->setSize(window.size());
        native = qobject_cast<rhi::SceneItem *>(find(root.get(), "nativeScene"));
        require(native, "Missing production SceneItem");
        native->setTelemetryEnabled(true);
        QObject::connect(native, &rhi::SceneItem::frameSubmitted, &window, [&](QJsonObject sample) {
            sample["openingTime"] = opening.time();
            samples.append(sample);
        });
        surface.show();
    }
    ~Driver()
    {
        root.reset();
        surface.hide();
        window.releaseResources();
    }
    void frames(int count, const std::function<void(int)> &prepare = { })
    {
        QEventLoop loop;
        QTimer deadline;
        deadline.setSingleShot(true);
        int rendered = 0;
        surface.connectFrames(&loop, [&] {
            if (rendered >= count)
                return;
            ++rendered;
            if (rendered == count)
                loop.quit();
            else {
                if (prepare)
                    prepare(rendered);
                surface.requestFrame();
            }
        });
        QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
        if (prepare)
            prepare(0);
        deadline.start(60000);
        surface.requestFrame();
        loop.exec();
        require(rendered == count, "Opening frame deadline");
        require(native->errorText().isEmpty(), "Production opening render failed");
        require(warnings.isEmpty(), "Production opening QML warnings");
    }
    void ready()
    {
        QEventLoop loop;
        QTimer deadline;
        deadline.setSingleShot(true);
        QObject::connect(&opening, &OpeningController::changed, &loop, [&] {
            if (!opening.preparing())
                loop.quit();
        });
        QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
        opening.replay();
        deadline.start(60000);
        surface.requestFrame();
        loop.exec();
        require(opening.film(), "Production renderer/cover readiness did not start the film");
        require(opening.time() == 0, "Preparation consumed film time");
    }
    void seek(double time)
    {
        while (opening.time() + 1e-9 < time && opening.film())
            motion.clock()->advance(std::min(.05, time - opening.time()));
    }
    void resize(QSize size, double dpr)
    {
        window.resize(size);
        root->setSize(size);
        if (OffscreenSurface::Enabled)
            surface.setDevicePixelRatio(dpr);
        frames(2);
    }
    QQuickRenderControl control;
    QQuickWindow window;
    OffscreenSurface surface;
    MotionController motion;
    SelectionController selection{ &motion };
    SceneAdapter scene{ motion.clock() };
    OpeningController opening{ &motion, &scene, &selection };
    LibrarySession library{ nullptr };
    QQmlEngine engine;
    std::unique_ptr<QQuickItem> root;
    rhi::SceneItem *native = nullptr;
    QStringList warnings;
    QJsonArray samples;
};
}
int reviewOpening(QGuiApplication &, const QString &output)
{
    QDir().mkpath(output);
    QJsonArray checks, captures, costs;
    bool passed = true;
    const auto check = [&](const QString &name, bool value) {
        passed &= value;
        checks.append(QJsonObject{ { "name", name }, { "passed", value } });
    };
    try {
        Driver d;
        QSignalSpy handed(&d.opening, &OpeningController::handedOff),
                completed(&d.opening, &OpeningController::completed);
        d.ready();
        d.frames(2);
        const auto warmDraws = rhi::sceneSubmissionStats().rendered.load();
        const auto warmSubmission = d.native->submission();
        d.frames(126, [&](int) {
            d.motion.clock()->advance(.05);
        });
        check("opaque-paper-zero-3d-submission",
              d.opening.time() < 6.32 && rhi::sceneSubmissionStats().rendered.load() == warmDraws
                      && d.native->submission() == warmSubmission);
        d.frames(108, [&](int) {
            d.motion.clock()->advance(.05);
        });
        check("one-film-handoff", handed.size() == 1 && !d.opening.film());
        check("locked-during-chrome-reveal", d.opening.active());
        d.frames(15, [&](int) {
            d.motion.clock()->advance(.05);
        });
        check("one-completion-after-720ms", !d.opening.active() && completed.size() == 1);
        check("resting-camera", d.scene.settled());
        check("focus-restored",
              d.window.activeFocusItem() != nullptr
                      && d.window.activeFocusItem() != find(d.root.get(), "openingSkip"));
        check("scene-resumed", rhi::sceneSubmissionStats().rendered.load() > warmDraws);
        d.ready();
        for (double time : { 6.4, 7.2, 9.0, 11.6 }) {
            d.seek(time);
            d.frames(2);
            const auto image = d.surface.grab();
            require(!image.isNull() && image.save(output + QString("/scene-t%1.png").arg(time)),
                    "Cannot save live opening scene output");
        }
        for (int i = 0; i < 15; ++i)
            d.motion.clock()->advance(.05);
        d.frames(2);
        const auto paintAtEnd = openingInkStats().paints.load();
        d.frames(3);
        check("exited-canvas-zero-repaint", openingInkStats().paints.load() == paintAtEnd);
        const auto playback = d.library.playbackTrackId();
        const QList<QSize> sizes{ { 1920, 1080 }, { 1280, 720 },  { 995, 858 },
                                  { 960, 900 },   { 930, 900 },   { 390, 844 },
                                  { 320, 720 },   { 2560, 1080 }, { 3440, 1440 } };
        for (bool night : { false, true }) {
            d.motion.setReducedMotion(true);
            d.scene.lighting()->setNight(night);
            d.motion.setReducedMotion(false);
            for (double dpr : { 1.0, 1.5, 2.0 }) {
                if (!OffscreenSurface::Enabled && dpr != d.window.devicePixelRatio())
                    continue;
                for (const auto &size : sizes) {
                    d.resize(size, dpr);
                    d.ready();
                    for (double time : { .95, 1.64, 2.2, 4.3 }) {
                        d.seek(time);
                        d.frames(2);
                        const QString name = QString("%1-%2x%3-dpr%4-t%5")
                                                     .arg(night ? "night" : "day")
                                                     .arg(size.width())
                                                     .arg(size.height())
                                                     .arg(dpr)
                                                     .arg(time);
                        const auto image = d.surface.grab();
                        require(!image.isNull() && image.save(output + '/' + name + ".png"),
                                "Cannot save actual opening output");
                        if (time == 4.3 && size == QSize(1280, 720) && dpr == 1) {
                            const auto paints = openingInkStats().paints.load();
                            d.frames(3, [&](int) {
                                d.motion.clock()->advance(.05);
                            });
                            check("static-slogan-zero-canvas-work",
                                  openingInkStats().paints.load() == paints);
                        }
                        const auto *skip = find(d.root.get(), "openingSkip");
                        check("bounded-skip-backdrop-" + name,
                              skip && skip->width() + 60 <= 220 && skip->height() + 60 <= 120);
                        const auto *headerNav = find(d.root.get(), "headerNavigation");
                        check("chrome-hidden-during-film-" + name,
                              headerNav && !headerNav->isVisible());
                        const auto *mark = find(d.root.get(), "openingMark");
                        check("brand-container-" + name,
                              mark
                                      && std::abs(mark->width()
                                                  - std::clamp(size.width() * .17, 155.0, 235.0))
                                              < 1e-6);
                        captures.append(
                                QJsonObject{ { "name", name },
                                             { "time", time },
                                             { "size", QJsonArray{ size.width(), size.height() } },
                                             { "dpr", d.window.effectiveDevicePixelRatio() } });
                    }
                    d.opening.skip();
                    for (int i = 0; i < 15; ++i)
                        d.motion.clock()->advance(.05);
                    d.frames(2);
                }
            }
        }
        // Compare the actual bounded brand component with Qt Quick Shape using identical trimmed paths.
        d.resize({ 1280, 720 }, 1);
        d.ready();
        d.seek(.95);
        d.frames(2);
        auto *ink = qobject_cast<OpeningInk *>(find(d.root.get(), "openingBrandInk"));
        require(ink, "Missing production brand painter");
        const auto compare = [&](bool quick) {
            QQmlComponent component(&d.engine,
                                    QUrl("qrc:/qt/qml/Yalahoo/Music/OpeningComparison.qml"));
            std::unique_ptr<QQuickItem> shape;
            if (quick) {
                shape.reset(qobject_cast<QQuickItem *>(component.createWithInitialProperties(
                        { { "source", QVariant::fromValue(ink) } })));
                require(bool(shape), "Cannot create Quick Shape comparison");
                shape->setParentItem(ink->parentItem());
                shape->setPosition(ink->position());
                ink->setVisible(false);
            }
            QElapsedTimer timer;
            const auto before = openingInkStats().paintNs.load();
            const auto paints = openingInkStats().paints.load();
            timer.start();
            d.frames(32, [&](int i) {
                ink->setTime(.7 + i * .02);
                if (shape)
                    shape->setProperty("paths", ink->comparisonPaths());
            });
            costs.append(QJsonObject{
                    { "api", quick ? "QtQuick.Shape.CurveRenderer" : "C++ Canvas Painter" },
                    { "frames", 32 },
                    { "guiAndQuickWallMs", timer.nsecsElapsed() / 1e6 },
                    { "canvasPaintMs", (openingInkStats().paintNs.load() - before) / 1e6 },
                    { "canvasPaints", qint64(openingInkStats().paints.load() - paints) },
                    { "gpuCompleteMs", QJsonValue::Null },
                    { "scope", "Local serial diagnostic; no FPS or cross-API image score" } });
            const auto image = d.surface.grab();
            require(image.save(output + (quick ? "/quick-shape.png" : "/canvas-painter.png")),
                    "Cannot save drawing comparison");
            shape.reset();
            ink->setVisible(true);
        };
        for (int sample = 0; sample < 3; ++sample) {
            compare(false);
            compare(true);
        }
        QKeyEvent tab(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
        QCoreApplication::sendEvent(&d.window, &tab);
        check("tab-focuses-skip", d.window.activeFocusItem() == find(d.root.get(), "openingSkip"));
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QCoreApplication::sendEvent(&d.window, &enter);
        check("enter-skips-film", !d.opening.film());
        d.motion.setReducedMotion(true);
        check("reduced-motion-completes-reveal", !d.opening.active());
        for (int cycle = 0; cycle < 10; ++cycle) {
            d.motion.setReducedMotion(false);
            d.ready();
            d.seek(cycle % 2 ? 2.2 : .95);
            d.frames(2);
            const auto paused = d.opening.time();
            d.motion.clock()->setActive(false);
            d.motion.clock()->advance(10);
            d.surface.hide();
            d.window.releaseResources();
            d.surface.show();
            d.frames(2);
            check("background-retains-time", d.opening.time() == paused);
            d.motion.clock()->setActive(true);
            d.motion.clock()->advance(10);
            check("resume-discards-first-delta", d.opening.time() == paused);
            const auto sceneBefore = rhi::sceneSubmissionStats().rendered.load();
            d.motion.setReducedMotion(true);
            d.frames(2);
            check("first-3d-frame-after-opaque-rebuild",
                  rhi::sceneSubmissionStats().rendered.load() > sceneBefore);
            check("renderer-release-after-replay", openingInkStats().renderers.load() == 0);
        }
        check("playback-isolated", d.library.playbackTrackId() == playback);
        check("window-policy", verification::windowPolicySatisfied(d.window));
        check("no-qml-warnings", d.warnings.isEmpty());
        QJsonObject report{ { "passed", passed },
                            { "checks", checks },
                            { "captures", captures },
                            { "costs", costs },
                            { "windowPolicy", verification::windowPolicy(d.window) },
                            { "offscreen", d.surface.diagnostics() },
                            { "samples", d.samples },
                            { "qtVersion", qVersion() },
                            { "backend", int(d.window.rendererInterface()->graphicsApi()) },
                            { "normalVsync", QJsonValue::Null },
                            { "physicalDisplay", QJsonValue::Null } };
        QSaveFile file(output + "/report.json");
        require(file.open(QIODevice::WriteOnly), "Cannot open opening report");
        file.write(QJsonDocument(report).toJson());
        require(file.commit(), "Cannot commit opening report");
    } catch (const std::exception &error) {
        qCritical("Opening audit: %s", error.what());
        return 3;
    }
    return passed ? 0 : 1;
}
