#include "ReviewSupport.h"
#include "RowLightController.h"
#include "SceneAdapter.h"
#include "MotionController.h"
#include "SelectionController.h"
#include <QQuickWindow>
#include "ExperimentChecks.h"
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
#include "JsonFile.h"
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QWheelEvent>

using namespace Qt::StringLiterals;

using namespace yalahoo;

namespace {
// Event-driven probe waits are bounded independently of injected fixture values.
constexpr int FrameDeadlineMilliseconds = 5000;

auto renderExperimentFrame(QJsonObject &lastSample, yalahoo::rhi::SceneItem *&native,
                           OffscreenSurface &surface, QQuickWindow &window,
                           MotionController &motion, SceneAdapter &scene)
{
    QEventLoop loop;
    QTimer watchdog;
    watchdog.setSingleShot(true);
    bool rendered = false;
    qint64 expectedInput = -1;
    const auto ready = [&] {
        return rendered && lastSample[u"inputRevision"_s].toInteger() == expectedInput;
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
    QQmlComponent component(&engine, QUrl(u"qrc:/qt/qml/Yalahoo/Music/LibraryReview.qml"_s));
    std::unique_ptr<QObject> object(component.createWithInitialProperties(
            { { u"librarySession"_s, QVariant::fromValue(&library) },
              { u"motionController"_s, QVariant::fromValue(&motion) },
              { u"sceneAdapter"_s, QVariant::fromValue(&scene) },
              { u"settingsController"_s, QVariant::fromValue(&settings) },
              { u"inputRouter"_s, QVariant::fromValue(&input) },
              { u"experimentsVisible"_s, true } }));
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
    auto *native =
            qobject_cast<rhi::SceneItem *>(yalahoo::review::findVisual(root, u"nativeScene"_s));
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
        return renderExperimentFrame(lastSample, native, surface, window, motion, scene);
    };
    QJsonArray checks;
    QJsonObject context;
    bool passed = true;
    const auto check = [&](const QString &name, bool okay) {
        auto result = context;
        result[u"check"_s] = name;
        result[u"passed"_s] = okay;
        checks.append(result);
        passed &= okay;
        if (!okay)
            qWarning() << "Experiment check failed:" << name << context;
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
                context = { { u"width"_s, size.width() },
                            { u"height"_s, size.height() },
                            { u"dpr"_s, dpr },
                            { u"night"_s, night } };
                check(u"render"_s, render());
                auto *panel = yalahoo::review::findVisual(root, u"experimentPanels"_s);
                auto *lighting = yalahoo::review::findVisual(root, u"lightingPanel"_s);
                auto *scroll = yalahoo::review::findVisual(root, u"experimentScroll"_s);
                auto *flickable =
                        scroll ? scroll->property("contentItem").value<QQuickItem *>() : nullptr;
                check(u"bounded-panel"_s,
                      panel && panel->x() >= 0 && panel->y() >= 0
                              && panel->mapToScene({ panel->width(), panel->height() }).x()
                                      <= size.width()
                              && panel->mapToScene({ panel->width(), panel->height() }).y()
                                      <= size.height());
                if (!night)
                    check(u"responsive-initial-fold"_s,
                          lighting
                                  && lighting->property("expanded").toBool()
                                          == (size.width() > 760 && size.height() > 650));
                if (lighting)
                    lighting->setProperty("expanded", true);
                check(u"expanded"_s, render());
                check(u"scroll-content"_s,
                      flickable && flickable->property("contentHeight").toDouble() > 0);
                const auto cell = selection.cell();
                const auto playback = library.playbackTrackId();
                if (panel) {
                    const auto point =
                            panel->mapToScene({ panel->width() / 2, panel->height() / 2 });
                    QWheelEvent wheel(point, window.mapToGlobal(point.toPoint()), { }, { 0, -120 },
                                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
                    check(u"wheel-owned"_s, !input.wheel(&wheel, &window));
                    QCoreApplication::sendEvent(&window, &wheel);
                }
                check(u"wheel-no-navigation"_s,
                      selection.cell() == cell && library.playbackTrackId() == playback);
                if (flickable) {
                    const auto bottom = std::max(0.0,
                                                 flickable->property("contentHeight").toDouble()
                                                         - flickable->height());
                    flickable->setProperty("contentY", bottom);
                }
                check(u"bottom-render"_s, render());
                auto *toggle = yalahoo::review::findVisual(root, u"diagnosticsToggle"_s);
                auto *label = toggle ? yalahoo::review::findVisual(toggle, u"checkBoxCaption"_s)
                                     : nullptr;
                check(u"theme-control-text"_s,
                      label
                              && label->property("color").value<QColor>()
                                      == QColor(night ? "#e9eef2" : "#232722"));
                if (size.width() == 320 && dpr == 1)
                    check(u"own-image"_s,
                          surface.grab().save(
                                  output
                                  + (night ? "/narrow-night.png"_L1 : "/narrow-day.png"_L1)));
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
    check(u"interaction-render"_s, render());
    auto *scroll = yalahoo::review::findVisual(root, u"experimentScroll"_s);
    auto *flickable = scroll ? scroll->property("contentItem").value<QQuickItem *>() : nullptr;
    auto *speed = yalahoo::review::findVisual(root, u"motionSpeedControl"_s);
    const auto cell = selection.cell();
    if (speed) {
        speed->forceActiveFocus();
        QTest::keyClick(&window, Qt::Key_Right);
    }
    check(u"speed-keyboard-owned"_s,
          speed && motion.clock()->speed() > 1 && selection.cell() == cell);
    click(yalahoo::review::findVisual(root, u"motionReset"_s));
    check(u"speed-reset"_s, motion.clock()->speed() == 1);
    check(u"resource-baseline"_s, render() && !lastSample.isEmpty());
    const auto generation = lastSample[u"resourceGeneration"_s],
               post = lastSample[u"postprocessRevision"_s];
    for (const auto &mode : { "baseline", "area", "guided", "hybrid" }) {
        scene.lighting()->configure({ { u"mode"_s, QString::fromUtf8(mode) },
                                      { u"strength"_s, 1.65 },
                                      { u"width"_s, 1.8 },
                                      { u"offset"_s, -.5 },
                                      { u"fill"_s, .6 },
                                      { u"transitionDuration"_s, .8 } });
        check(u"mode-render"_s, render());
        check(u"uniform-only"_s,
              lastSample[u"resourceGeneration"_s] == generation
                      && lastSample[u"postprocessRevision"_s] == post);
        check(u"actual-mode"_s,
              native->input().rowSettings.x() == int(scene.lighting()->ribbon.settings.mode));
    }
    if (flickable)
        flickable->setProperty("contentY", 180);
    check(u"comparison-ready"_s, render());
    auto *compare = yalahoo::review::findVisual(root, u"lightingCompare"_s);
    if (compare) {
        compare->forceActiveFocus();
        QTest::keyPress(&window, Qt::Key_Space);
    }
    check(u"hold-original"_s,
          scene.lighting()->comparing()
                  && scene.lighting()->ribbon.settings.mode == RowLightingMode::Baseline);
    if (compare)
        QTest::keyRelease(&window, Qt::Key_Space);
    check(u"release-restores"_s,
          !scene.lighting()->comparing()
                  && scene.lighting()->ribbon.settings.mode == RowLightingMode::Hybrid);
    scene.lighting()->setComparing(true);
    root->setProperty("experimentsVisible", false);
    check(u"close-render"_s, render());
    check(u"close-restores"_s, !scene.lighting()->comparing() && !native->diagnosticsEnabled());
    root->setProperty("experimentsVisible", true);
    check(u"reopen"_s, render());
    scene.lighting()->resetSettings();
    auto *panel = yalahoo::review::findVisual(root, u"experimentPanels"_s);
    if (panel)
        panel->setProperty("diagnostics", true);
    check(u"diagnostics-on"_s, render() && native->diagnosticsEnabled());
    QElapsedTimer sampling;
    sampling.start();
    while (native->diagnosticValues().isEmpty() && sampling.elapsed() < FrameDeadlineMilliseconds)
        if (!render())
            break;
    const auto values = native->diagnosticValues();
    check(u"diagnostic-batch"_s,
          !values.isEmpty() && values[u"cpu"_s].toMap()[u"count"_s].toInt() > 0
                  && values[u"elapsedMs"_s].toDouble() >= 500
                  && values[u"cpu"_s].toMap()[u"count"_s].toInt() <= 256
                  && values[u"qtFrameIntervals"_s].toMap().isEmpty() == OffscreenSurface::Enabled);
    QSignalSpy published(native, &rhi::SceneItem::diagnosticValuesChanged);
    if (panel)
        panel->setProperty("diagnostics", false);
    check(u"diagnostics-off"_s,
          !native->diagnosticsEnabled() && native->diagnosticValues().isEmpty());
    const auto disabled = published.size();
    for (int i = 0; i < 4; ++i)
        render();
    check(u"no-late-diagnostics"_s, published.size() == disabled);
    check(u"qml-warnings"_s, warnings.isEmpty());
    check(u"window-policy"_s, verification::windowPolicySatisfied(window));
    const auto windowPolicy = verification::windowPolicy(window);
    surface.hide();
    root->setParentItem(nullptr);
    object.reset();
    window.releaseResources();
    check(u"renderers-released"_s,
          QTest::qWaitFor(
                  [] {
                      return rhi::sceneSubmissionStats().renderers.load() == 0;
                  },
                  5000));
    const bool saved = yalahoo::writeJsonFile(
            output + "/report.json"_L1,
            QJsonDocument(
                    QJsonObject{ { u"passed"_s, passed },
                                 { u"checks"_s, checks },
                                 { u"diagnosticSample"_s, QJsonObject::fromVariantMap(values) },
                                 { u"windowPolicy"_s, windowPolicy },
                                 { u"offscreen"_s, OffscreenSurface::Enabled } }));
    return saved && passed ? 0 : 5;
}
