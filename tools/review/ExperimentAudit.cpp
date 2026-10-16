#include "ExperimentAudit.h"
#include "OffscreenSurface.h"
#include "DemoLibrary.h"
#include "OfflineMetadataProvider.h"
#include "LibrarySession.h"
#include "SettingsController.h"
#include "SceneItem.h"
#include "VerificationWindow.h"
#include <QDir>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QWheelEvent>
using namespace yalahoo;
namespace {
QQuickItem *visual(QQuickItem *item, const QString &name)
{
    if (item->objectName() == name)
        return item;
    for (auto *child : item->childItems())
        if (auto *found = visual(child, name))
            return found;
    return nullptr;
}
}
int reviewExperiments(const QString &output)
{
    QDir().mkpath(output);
    MotionController motion;
    OfflineMetadataProvider provider;
    LibrarySession library(&provider);
    library.setSnapshot(demonstrationLibrary());
    SelectionController selection(&motion);
    selection.setColumns(library.albums()->order().columns, library.snapshot());
    SceneAdapter scene(motion.clock());
    scene.setSelection(&selection);
    SettingsController settings(nullptr, &library, &motion, &scene, &selection);
    // Inspect settled palettes; animated theme trajectories are covered by lighting's oracle.
    motion.setReducedMotion(true);
    scene.setReducedMotion(true);
    InputRouter input(&selection);
    QQmlEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    QQmlComponent component(&engine, QUrl("qrc:/qt/qml/Yalahoo/Music/LibraryReview.qml"));
    std::unique_ptr<QObject> object(component.createWithInitialProperties(
            { { "librarySession", QVariant::fromValue(&library) },
              { "motionController", QVariant::fromValue(&motion) },
              { "sceneAdapter", QVariant::fromValue(&scene) },
              { "settingsController", QVariant::fromValue(&settings) },
              { "inputRouter", QVariant::fromValue(&input) },
              { "experimentsVisible", true } }));
    auto *root = qobject_cast<QQuickItem *>(object.get());
    if (!root) {
        qWarning() << component.errors();
        return 2;
    }
    QQuickRenderControl control;
    QQuickWindow window(OffscreenSurface::Enabled ? &control : nullptr);
    window.setPersistentGraphics(false);
    window.setPersistentSceneGraph(false);
    verification::prepareWindow(window, true);
    OffscreenSurface surface(window, control);
    root->setParentItem(window.contentItem());
    input.attach(&window);
    auto *native = qobject_cast<rhi::SceneItem *>(visual(root, "nativeScene"));
    if (!native)
        return 3;
    QJsonObject lastSample;
    QObject::connect(native, &rhi::SceneItem::frameSubmitted, root, [&](const QJsonObject &sample) {
        lastSample = sample;
    });
    native->setTelemetryEnabled(true); // Review-only resource inventory; panel never enables this.
    surface.show();
    motion.clock()->setActive(true);
    motion.clock()->advance(0);
    const auto render = [&] {
        QEventLoop loop;
        QTimer watchdog;
        watchdog.setSingleShot(true);
        bool rendered = false;
        qint64 expectedInput = -1;
        const auto ready = [&] {
            return rendered && lastSample["inputRevision"].toInteger() == expectedInput;
        };
        QObject::connect(native, &rhi::SceneItem::frameSubmitted, &loop, [&] {
            if (ready())
                loop.quit();
        });
        const auto done = [&] {
            rendered = true;
            if (ready())
                loop.quit();
        };
        QMetaObject::Connection connection;
        if (OffscreenSurface::Enabled)
            connection = QObject::connect(&surface, &OffscreenSurface::frameRendered, &loop, done);
        else
            connection = QObject::connect(&window, &QQuickWindow::frameSwapped, &loop, done,
                                          Qt::QueuedConnection);
        QObject::connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
        motion.clock()->advance(1.0 / 60);
        scene.recordInput();
        expectedInput = qint64(scene.frame().inputRevision);
        surface.requestFrame();
        watchdog.start(5000);
        loop.exec();
        QObject::disconnect(connection);
        return ready();
    };
    QJsonArray checks;
    QJsonObject context;
    bool passed = true;
    const auto check = [&](const QString &name, bool okay) {
        auto result = context;
        result["check"] = name;
        result["passed"] = okay;
        checks.append(result);
        passed &= okay;
        if (!okay)
            qWarning() << "Experiment audit failed:" << name << context;
    };
    auto click = [&](QQuickItem *item) {
        if (item)
            QTest::mouseClick(
                    &window, Qt::LeftButton, Qt::NoModifier,
                    item->mapToScene({ item->width() / 2, item->height() / 2 }).toPoint());
    };
    const QList<QSize> sizes{ { 1920, 1080 }, { 1280, 720 },  { 995, 858 },
                              { 960, 900 },   { 930, 900 },   { 390, 844 },
                              { 320, 720 },   { 2560, 1080 }, { 3440, 1440 } };
    const auto dprs = OffscreenSurface::Enabled ? QList<double>{ 1, 1.5, 2 }
                                                : QList<double>{ window.devicePixelRatio() };
    for (auto dpr : dprs) {
        if (OffscreenSurface::Enabled)
            surface.setDevicePixelRatio(dpr);
        for (auto size : sizes) {
            root->setProperty("experimentsVisible", false);
            root->setProperty("experimentsVisible", true);
            window.resize(size);
            root->setSize(size);
            for (bool night : { false, true }) {
                scene.lighting()->setNight(night);
                context = { { "width", size.width() },
                            { "height", size.height() },
                            { "dpr", dpr },
                            { "night", night } };
                check("render", render());
                auto *panel = visual(root, "experimentPanels");
                auto *lighting = visual(root, "lightingPanel");
                auto *scroll = visual(root, "experimentScroll");
                auto *flickable =
                        scroll ? scroll->property("contentItem").value<QQuickItem *>() : nullptr;
                check("bounded-panel",
                      panel && panel->x() >= 0 && panel->y() >= 0
                              && panel->mapToScene({ panel->width(), panel->height() }).x()
                                      <= size.width()
                              && panel->mapToScene({ panel->width(), panel->height() }).y()
                                      <= size.height());
                if (!night)
                    check("responsive-initial-fold",
                          lighting
                                  && lighting->property("expanded").toBool()
                                          == (size.width() > 760 && size.height() > 650));
                if (lighting)
                    lighting->setProperty("expanded", true);
                check("expanded", render());
                check("scroll-content",
                      flickable && flickable->property("contentHeight").toDouble() > 0);
                const auto cell = selection.cell();
                const auto playback = library.playbackTrackId();
                if (panel) {
                    const auto point =
                            panel->mapToScene({ panel->width() / 2, panel->height() / 2 });
                    QWheelEvent wheel(point, window.mapToGlobal(point.toPoint()), { }, { 0, -120 },
                                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
                    check("wheel-owned", !input.wheel(&wheel, &window));
                    QCoreApplication::sendEvent(&window, &wheel);
                }
                check("wheel-no-navigation",
                      selection.cell() == cell && library.playbackTrackId() == playback);
                if (flickable) {
                    const auto bottom = std::max(0.0,
                                                 flickable->property("contentHeight").toDouble()
                                                         - flickable->height());
                    flickable->setProperty("contentY", bottom);
                }
                check("bottom-render", render());
                auto *toggle = visual(root, "diagnosticsToggle");
                auto *label = toggle ? visual(toggle, "checkBoxCaption") : nullptr;
                check("theme-control-text",
                      label
                              && label->property("color").value<QColor>()
                                      == QColor(night ? "#e9eef2" : "#232722"));
                if (size.width() == 320 && dpr == 1)
                    check("own-image",
                          surface.grab().save(output
                                              + (night ? "/narrow-night.png" : "/narrow-day.png")));
                if (flickable)
                    flickable->setProperty("contentY", 0);
            }
        }
    }
    context = { };
    window.resize(1280, 720);
    root->setSize({ 1280, 720 });
    if (OffscreenSurface::Enabled)
        surface.setDevicePixelRatio(1);
    check("interaction-render", render());
    auto *scroll = visual(root, "experimentScroll");
    auto *flickable = scroll ? scroll->property("contentItem").value<QQuickItem *>() : nullptr;
    auto *speed = visual(root, "motionSpeedControl");
    const auto cell = selection.cell();
    if (speed) {
        speed->forceActiveFocus();
        QTest::keyClick(&window, Qt::Key_Right);
    }
    check("speed-keyboard-owned", speed && motion.clock()->speed() > 1 && selection.cell() == cell);
    click(visual(root, "motionReset"));
    check("speed-reset", motion.clock()->speed() == 1);
    check("resource-baseline", render() && !lastSample.isEmpty());
    const auto generation = lastSample["resourceGeneration"],
               post = lastSample["postprocessRevision"];
    for (const auto &mode : { "baseline", "area", "guided", "hybrid" }) {
        scene.lighting()->configure({ { "mode", mode },
                                      { "strength", 1.65 },
                                      { "width", 1.8 },
                                      { "offset", -.5 },
                                      { "fill", .6 },
                                      { "transitionDuration", .8 } });
        check("mode-render", render());
        check("uniform-only",
              lastSample["resourceGeneration"] == generation
                      && lastSample["postprocessRevision"] == post);
        check("actual-mode",
              native->input().rowSettings.x() == int(scene.lighting()->ribbon.settings.mode));
    }
    if (flickable)
        flickable->setProperty("contentY", 180);
    check("comparison-ready", render());
    auto *compare = visual(root, "lightingCompare");
    if (compare) {
        compare->forceActiveFocus();
        QTest::keyPress(&window, Qt::Key_Space);
    }
    check("hold-original",
          scene.lighting()->comparing()
                  && scene.lighting()->ribbon.settings.mode == RowLightingMode::Baseline);
    if (compare)
        QTest::keyRelease(&window, Qt::Key_Space);
    check("release-restores",
          !scene.lighting()->comparing()
                  && scene.lighting()->ribbon.settings.mode == RowLightingMode::Hybrid);
    scene.lighting()->setComparing(true);
    root->setProperty("experimentsVisible", false);
    check("close-render", render());
    check("close-restores", !scene.lighting()->comparing() && !native->diagnosticsEnabled());
    root->setProperty("experimentsVisible", true);
    check("reopen", render());
    scene.lighting()->resetSettings();
    auto *panel = visual(root, "experimentPanels");
    if (panel)
        panel->setProperty("diagnostics", true);
    check("diagnostics-on", render() && native->diagnosticsEnabled());
    QElapsedTimer sampling;
    sampling.start();
    while (native->diagnosticValues().isEmpty() && sampling.elapsed() < 5000)
        if (!render())
            break;
    const auto values = native->diagnosticValues();
    check("diagnostic-batch",
          !values.isEmpty() && values["cpu"].toMap()["count"].toInt() > 0
                  && values["elapsedMs"].toDouble() >= 500
                  && values["cpu"].toMap()["count"].toInt() <= 256
                  && values["qtFrameIntervals"].toMap().isEmpty() == OffscreenSurface::Enabled);
    QSignalSpy published(native, &rhi::SceneItem::diagnosticValuesChanged);
    if (panel)
        panel->setProperty("diagnostics", false);
    check("diagnostics-off", !native->diagnosticsEnabled() && native->diagnosticValues().isEmpty());
    const auto disabled = published.size();
    for (int i = 0; i < 4; ++i)
        render();
    check("no-late-diagnostics", published.size() == disabled);
    check("qml-warnings", warnings.isEmpty());
    check("window-policy", verification::windowPolicySatisfied(window));
    const auto windowPolicy = verification::windowPolicy(window);
    surface.hide();
    root->setParentItem(nullptr);
    object.reset();
    window.releaseResources();
    check("renderers-released",
          QTest::qWaitFor(
                  [] {
                      return rhi::sceneSubmissionStats().renderers.load() == 0;
                  },
                  5000));
    QSaveFile file(output + "/report.json");
    if (!file.open(QIODevice::WriteOnly))
        return 4;
    file.write(
            QJsonDocument(QJsonObject{ { "passed", passed },
                                       { "checks", checks },
                                       { "diagnosticSample", QJsonObject::fromVariantMap(values) },
                                       { "windowPolicy", windowPolicy },
                                       { "offscreen", OffscreenSurface::Enabled } })
                    .toJson());
    return file.commit() && passed ? 0 : 5;
}
