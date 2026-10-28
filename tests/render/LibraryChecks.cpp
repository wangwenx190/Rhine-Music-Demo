#include "SettingsResourceChecks.h"
#include "LibraryChecks.h"
#include "RenderQuality.h"
#include "SelectionLighting.h"
#include "LayoutMetrics.h"
#include "../support/LibraryFixture.h"
#include "ReviewSupport.h"
#include "LibrarySession.h"
#include "MotionController.h"
#include "SelectionController.h"
#include <QQuickWindow>
#include "PresentationController.h"
#include "PreferencesStore.h"
#include "MetadataPresentation.h"
#include "PlaybackController.h"
#include "BackgroundMusicController.h"
#include "InterfaceSoundController.h"
#include "AudioFixture.h"
#include <QTemporaryDir>
#include "OffscreenSurface.h"
#include "DetailPorts.h"
#include "DetailModel.h"
#include "TrackFocusController.h"
#include "PanelController.h"
#include "SettingsController.h"
#include <QTimer>
#include <QEventLoop>
#include "SceneAdapter.h"
#include "SceneItem.h"
#include "OverviewController.h"
#include "InputRouter.h"
#include "RulerController.h"
#include "OfflineMetadataProvider.h"
#include "VerificationWindow.h"
#include <QDir>
#include <QFile>
#include <QCryptographicHash>
#include <QSysInfo>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include "JsonFile.h"
#include <QSignalSpy>
#include <QTest>
#include <atomic>

using namespace Qt::StringLiterals;

using namespace yalahoo;
namespace {
// Event-driven probe waits are bounded independently of injected fixture values.
constexpr int FrameDeadlineMilliseconds = 5000;
constexpr int MediaDeadlineMilliseconds = 10000;
constexpr int InputDeadlineMilliseconds = 2000;
constexpr int ModelDeadlineMilliseconds = 3000;

// Bounds for asynchronous completion and event delivery; fixture values remain independent.
constexpr int EventPollMilliseconds = 5;

bool waitForSearch(SearchListModel &model)
{
    return QTest::qWaitFor(
            [&model] {
                return !model.searching();
            },
            ModelDeadlineMilliseconds);
}

// Test-only injection. No networking objects or production registration.
class SampleProvider final : public IMetadataProvider
{
    Q_DISABLE_COPY_MOVE(SampleProvider)
public:
    SampleProvider() = default;
    ProviderStatus status = ProviderStatus::Matched;
    bool enabled = false;
    bool lastQueryEnabled = false;
    int introductionRequests = 0;
    ProviderCapabilities capabilities() const override
    {
        return { enabled, true, true, true, false };
    }
    void setEnabled(bool value)
    {
        enabled = value;
        Q_EMIT capabilitiesChanged();
    }
    bool request(AlbumQuery query, RequestToken token, MetadataKind kind) override
    {
        if (kind == MetadataKind::Introduction) {
            lastQueryEnabled = query.enabled;
            ++introductionRequests;
        }
        deliver(token, kind);
        return true;
    }
    void cancel(RequestToken) override
    {
    }
    void deliver(RequestToken token, MetadataKind kind)
    {
        ProviderResult result;
        result.requestId = token.requestId;
        result.generation = token.generation;
        result.albumId = token.albumId;
        result.kind = kind;
        result.status = status;
        result.payload = {
            { u"description"_s,
              u"Injected introduction \u00b7 \u672c\u673a\u6d4b\u8bd5\u8d44\u6599"_s }
        };
        result.source = { { u"name"_s, u"Offline test provider"_s } };
        result.error = status == ProviderStatus::Error ? "Injected error"_L1 : ""_L1;
        QTimer::singleShot(0, this, [this, result] {
            Q_EMIT resultReady(result);
        });
    }
};

auto collectSettingsSample(const qint64 &previous, const quint64 &inputRevision,
                           const QSize &expected, bool &ready, QEventLoop &loop,
                           const QJsonObject &sample)
{
    if (sample[u"submission"_s].toInteger() > previous
        && sample[u"inputRevision"_s].toInteger() == qint64(inputRevision)
        && sample[u"targetWidth"_s].toInt() == expected.width()
        && sample[u"targetHeight"_s].toInt() == expected.height()) {
        ready = true;
        loop.quit();
    }
}

auto renderSettingsFrame(yalahoo::rhi::SceneItem *&settingsScene, SceneAdapter &scene,
                         QJsonObject &lastSettingsSample, auto &render, OffscreenSurface &surface)
{
    if (!settingsScene)
        return false;
    const auto expected = scene.quality()->targetSize();
    const auto previous = lastSettingsSample[u"submission"_s].toInteger();
    // One explicit Review action receipt, so a static scene submits this observation.
    scene.recordInput();
    const auto inputRevision = scene.frame().inputRevision;
    QEventLoop loop;
    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
    bool ready = false;
    const auto connection = QObject::connect(
            settingsScene, &rhi::SceneItem::frameSubmitted, &loop, [&](const QJsonObject &sample) {
                return collectSettingsSample(previous, inputRevision, expected, ready, loop,
                                             sample);
            });
    const bool rendered = render();
    if (!ready) {
        watchdog.start(5000);
        surface.requestFrame();
        loop.exec();
    }
    QObject::disconnect(connection);
    return rendered && ready;
}

auto reviewLibraryObserveFrame(const std::shared_ptr<std::atomic<bool>> &deviceCaptured,
                               QQuickRenderControl &control, QQuickWindow &window)
{
    if (deviceCaptured->exchange(true))
        return;
    auto *api = OffscreenSurface::Enabled
            ? control.rhi()
            : static_cast<QRhi *>(window.rendererInterface()->getResource(
                      &window, QSGRendererInterface::RhiResource));
    const QVariantMap info{
        { u"backend"_s, api ? QString::fromUtf8(api->backendName()) : QString() },
        { u"gpu"_s, api ? QString::fromUtf8(api->driverInfo().deviceName) : QString() }
    };
    QMetaObject::invokeMethod(
            &window,
            [&window, info] {
                window.setProperty("libraryChecksDevice", info);
            },
            Qt::QueuedConnection);
}

struct NativeFrames final
{
    std::atomic<quint64> synchronized{ 0 };
    std::atomic<quint64> presented{ 0 };
};
auto reviewLibraryRender(const std::shared_ptr<NativeFrames> &nativeFrames,
                         OffscreenSurface &surface, QSignalSpy &rendered)
{
    if (!OffscreenSurface::Enabled) {
        // A swap from an already synchronized frame cannot acknowledge a new GUI change.
        const auto before = nativeFrames->synchronized.load();
        surface.requestFrame();
        return QTest::qWaitFor(
                [&] {
                    return nativeFrames->presented.load() > before;
                },
                5000);
    }
    const int before = rendered.size();
    surface.requestFrame();
    QElapsedTimer wait;
    wait.start();
    while (rendered.size() == before && wait.elapsed() < FrameDeadlineMilliseconds)
        QTest::qWait(EventPollMilliseconds);
    return rendered.size() > before;
}

auto verifySettingsInputs(auto &check, auto &renderSettingsTarget, QJsonObject &lastSettingsSample,
                          yalahoo::rhi::SceneItem *&settingsScene,
                          QJsonArray &settingsResourceSamples, QQuickWindow &window,
                          QQuickItem *&scroll, auto &render, SettingsController &settings,
                          MotionController &motion, SceneAdapter &scene, OffscreenSurface &surface,
                          const QString &output, SampleProvider &injected, LibrarySession &library,
                          QQuickItem *&query, LibrarySnapshot &snapshot)
{
    check(u"settings-resource-sample"_s, renderSettingsTarget() && !lastSettingsSample.isEmpty());
    const auto baselineResources = lastSettingsSample;
    const auto atlasGeneration = settingsScene ? settingsScene->input().atlasGeneration : 0;
    settingsResourceSamples.append(baselineResources);
    const auto focusSetting = [&](const QString &name) {
        auto *control = yalahoo::review::findVisual(window.contentItem(), name);
        if (control && scroll) {
            const double y = control->mapToItem(scroll, QPointF()).y();
            scroll->setProperty("contentY",
                                std::clamp(scroll->property("contentY").toDouble() + y - 20, 0.,
                                           std::max(0.,
                                                    scroll->property("contentHeight").toDouble()
                                                            - scroll->height())));
            control->forceActiveFocus();
            render();
        }
        return control;
    };
    auto *single = focusSetting(u"settingsSingleInstance"_s);
    check(u"single-instance-control-present"_s, single != nullptr);
    if (single)
        QTest::keyClick(&window, Qt::Key_Space);
    check(u"single-instance-qt-input"_s, !settings.values()[u"singleInstance"_s].toBool());
    settings.update(u"singleInstance"_s, true);
    auto *speed = focusSetting(u"settingsSpeedControl"_s);
    check(u"speed-control-present"_s, speed != nullptr);
    if (speed)
        QTest::keyClick(&window, Qt::Key_Right);
    check(u"speed-qt-input"_s, std::abs(motion.clock()->speed() - 1.05) < 1e-9);
    check(u"speed-control-range"_s,
          speed && speed->property("from").toDouble() == .25
                  && speed->property("to").toDouble() == 3.0);
    check(u"speed-resource-render"_s, renderSettingsTarget());
    check(u"speed-keeps-resources"_s,
          lastSettingsSample[u"resourceGeneration"_s] == baselineResources[u"resourceGeneration"_s]
                  && lastSettingsSample[u"postprocessRevision"_s]
                          == baselineResources[u"postprocessRevision"_s]);
    settings.update(u"motionSpeed"_s, 1.0);
    if (scroll)
        scroll->setProperty("contentY", 0);
    render();
    auto *theme = yalahoo::review::findVisual(window.contentItem(), u"settingsNightTheme"_s);
    check(u"theme-control-present"_s, theme != nullptr);
    if (theme) {
        theme->forceActiveFocus();
        QTest::keyClick(&window, Qt::Key_Space);
        yalahoo::review::checkSettingsTransition(check, renderSettingsTarget, lastSettingsSample,
                                                 baselineResources, u"night-theme"_s);
        check(u"theme-qt-input"_s, scene.lighting()->night());
        settings.update(u"theme"_s, u"day"_s);
        yalahoo::review::checkSettingsTransition(check, renderSettingsTarget, lastSettingsSample,
                                                 baselineResources, u"day-theme"_s);
    }
    auto *qualitySlider = focusSetting(u"settingsdepthOfFieldControl"_s);
    check(u"quality-control-present"_s, qualitySlider != nullptr);
    if (qualitySlider)
        QTest::keyClick(&window, Qt::Key_Right);
    check(u"quality-qt-input"_s, scene.quality()->values()[u"depthOfField"_s].toInt() == 105);
    check(u"quality-resource-render"_s, renderSettingsTarget());
    check(u"effect-keeps-native-target"_s,
          lastSettingsSample[u"resourceGeneration"_s].toInteger()
                          == baselineResources[u"resourceGeneration"_s].toInteger()
                  && settingsScene && settingsScene->input().atlasGeneration == atlasGeneration);
    settingsResourceSamples.append(lastSettingsSample);
    check(u"quality-own-image"_s, surface.grab().save(output + "/settings-quality.png"_L1));
    if (qualitySlider)
        QTest::keyClick(&window, Qt::Key_Left);
    check(u"quality-resource-restore"_s, renderSettingsTarget());
    settingsResourceSamples.append(lastSettingsSample);
    check(u"quality-resource-return"_s,
          lastSettingsSample[u"resources"_s].toObject()[u"estimatedCurrentBytes"_s]
                  == baselineResources[u"resources"_s].toObject()[u"estimatedCurrentBytes"_s]);
    if (scroll)
        scroll->setProperty("contentY", 0);
    render();
    for (auto status :
         { ProviderStatus::Matched, ProviderStatus::Uncertain, ProviderStatus::Error }) {
        injected.status = status;
        library.requestMetadata(library.navigationAlbumId(), u"introduction"_s);
        QElapsedTimer timer;
        timer.start();
        while (library.metadataFor(library.navigationAlbumId(), u"introduction"_s)[u"status"_s]
                               .toString()
                       != statusName(status)
               && timer.elapsed() < InputDeadlineMilliseconds)
            QTest::qWait(EventPollMilliseconds);
        check(u"injected-status-render"_s, render());
        auto *state =
                yalahoo::review::findVisual(window.contentItem(), u"introductionMetadataState"_s);
        check(u"injected-status-ui"_s,
              state
                      && state->property("text").toString().contains(
                              metadataStatusText(statusName(status))));
        auto *content =
                yalahoo::review::findVisual(window.contentItem(), u"introductionMetadataContent"_s);
        check(u"injected-content-ui"_s,
              content && content->property("text").toString().contains(u"Injected introduction"_s));
    }
    injected.setEnabled(true);
    check(u"future-capability-render"_s, render());
    check(u"future-capability-enables-query"_s, query && query->isEnabled());
    const int previousRequests = injected.introductionRequests;
    injected.status = ProviderStatus::Matched;
    if (query) {
        query->forceActiveFocus();
        QTest::keyClick(&window, Qt::Key_Space);
    }
    QElapsedTimer queryTimer;
    queryTimer.start();
    while (library.metadataBusy() && queryTimer.elapsed() < ModelDeadlineMilliseconds)
        QTest::qWait(EventPollMilliseconds);
    check(u"current-query-qt-input"_s,
          injected.lastQueryEnabled && injected.introductionRequests == previousRequests + 1
                  && library.metadataFor(library.navigationAlbumId(),
                                         u"introduction"_s)[u"status"_s]
                                  .toString()
                          == "Matched"_L1);
    auto *batchQuery =
            yalahoo::review::findVisual(window.contentItem(), u"introductionMetadataBatch"_s);
    check(u"batch-query-enabled"_s, render() && batchQuery && batchQuery->isEnabled());
    if (batchQuery) {
        batchQuery->forceActiveFocus();
        QTest::keyClick(&window, Qt::Key_Space);
    }
    queryTimer.restart();
    while (library.metadataProgress()[u"active"_s].toBool()
           && queryTimer.elapsed() < ModelDeadlineMilliseconds)
        QTest::qWait(EventPollMilliseconds);
    check(u"batch-query-qt-input"_s,
          library.metadataProgress()[u"kind"_s].toString() == "introduction"_L1
                  && library.metadataProgress()[u"completed"_s].toInt() == snapshot.albums.size());
    injected.setEnabled(false);
    check(u"offline-capability-restored"_s, render() && query && !query->isEnabled());
    check(u"batch-start"_s, library.requestMetadataBatch(u"credits"_s));
    QElapsedTimer timer;
    timer.start();
    while (library.metadataProgress()[u"active"_s].toBool()
           && timer.elapsed() < ModelDeadlineMilliseconds)
        QTest::qWait(EventPollMilliseconds);
    check(u"batch-progress"_s,
          !library.metadataProgress()[u"active"_s].toBool()
                  && library.metadataProgress()[u"completed"_s].toInt() == snapshot.albums.size());
}
}

int reviewLibrary(QGuiApplication &, const QString &output, bool playbackVerification,
                  bool settingsVerification, bool representative)
{
    QDir().mkpath(output);
    MotionController motion;
    SelectionController selection(&motion);
    SceneAdapter scene(motion.clock());
    OfflineMetadataProvider provider;
    SampleProvider injected;
    LibrarySession library(settingsVerification ? static_cast<IMetadataProvider *>(&injected)
                                                : &provider);
    QTemporaryDir audioDirectory;
    QUrl audioSource;
    if (playbackVerification) {
        QFile audioFile(audioDirectory.path() + u"/\u533f\u540d \u6b4c\u66f2 #.wav"_s);
        if (!audioFile.open(QIODevice::WriteOnly) || audioFile.write(test::wave(30000)) <= 0)
            return 2;
        audioSource = QUrl::fromLocalFile(audioFile.fileName());
    }
    LibrarySnapshot snapshot;
    snapshot.genres = { { u"g"_s, u"\u6d41\u6d3e\u4e00"_s, { }, { }, { } },
                        { u"h"_s, u"<b>\u6d41\u6d3e\u4e8c</b>"_s, { }, { }, { } } };
    for (int i = 0; i < 20; ++i) {
        Album album;
        album.id = u"album-%1"_s.arg(i);
        album.title = u"\u6536\u85cf\u4e13\u8f91 %1 \u00b7 Anonymous Archive"_s.arg(i);
        album.artist = i < 10 ? "Artist A"_L1 : "Artist B"_L1;
        album.genreId = i < 10 ? "g"_L1 : "h"_L1;
        for (int j = 0; j < 200; ++j) {
            Track track;
            track.id = u"track-%1"_s.arg(j);
            track.albumId = album.id;
            track.title = u"Song %1 \u00b7 \u6536\u85cf\u66f2\u76ee \U0001f3b5"_s.arg(j);
            track.artist = album.artist;
            track.sourceUrl = audioSource;
            track.duration = 185;
            track.discNumber = j < 100 ? 1 : 2;
            album.tracks.append(track);
        }
        snapshot.albums.append(album);
    }
    library.setSnapshot(snapshot);
    library.sort(u"artist"_s);
    selection.setColumns(library.albums()->order().columns, library.snapshot());
    test::bindLibrarySelection(library, selection, scene);
    library.setPlaybackIds(u"album-0"_s, u"track-0"_s);
    PresentationController presentation(motion.clock(), detailPorts(scene, selection, library));
    DetailModel detail;
    QObject::connect(&presentation, &PresentationController::prepared, &detail, [&] {
        detail.prepare(library);
    });
    TrackFocusController focus(&library, &presentation, motion.clock());
    PanelController panels(motion.clock());
    InputRouter input(&selection);
    RulerController ruler(&motion, &selection);
    OverviewController overview(&motion);
    overview.bind(&scene, &selection, &presentation);
    QObject::connect(&focus, &TrackFocusController::navigationRequested, &overview, [&] {
        overview.setActive(false);
    });
    const auto updateInput = [&] {
        input.setEnabled(presentation.archiveEnabled() && !panels.surface()->shown());
    };
    QObject::connect(panels.surface(), &SurfaceTransition::changed, &input, updateInput);
    QObject::connect(&presentation, &PresentationController::changed, &input, updateInput);
    QQmlEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    BackgroundMusicController background;
    InterfaceSoundController sounds;
    std::unique_ptr<PlaybackController> song;
    if (playbackVerification) {
        song = std::make_unique<PlaybackController>(&library);
        song->setVolume(.01);
        background.setEnabled(false);
        song->setBackgroundMusic(&background);
    }
    PreferencesStore preferences(audioDirectory.path());
    SettingsController settings(settingsVerification ? &preferences : nullptr, &library, &motion,
                                &scene, &selection);
    QQmlComponent component(&engine, QUrl(u"qrc:/qt/qml/Yalahoo/Music/LibraryReview.qml"_s));
    QVariantMap properties{ { u"librarySession"_s, QVariant::fromValue(&library) },
                            { u"motionController"_s, QVariant::fromValue(&motion) },
                            { u"sceneAdapter"_s, QVariant::fromValue(&scene) },
                            { u"presentationController"_s, QVariant::fromValue(&presentation) },
                            { u"detailModel"_s, QVariant::fromValue(&detail) },
                            { u"trackFocusController"_s, QVariant::fromValue(&focus) },
                            { u"panelController"_s, QVariant::fromValue(&panels) },
                            { u"inputRouter"_s, QVariant::fromValue(&input) },
                            { u"rulerController"_s, QVariant::fromValue(&ruler) },
                            { u"overviewController"_s, QVariant::fromValue(&overview) } };
    if (settingsVerification)
        properties.insert(u"settingsController"_s, QVariant::fromValue(&settings));
    if (song) {
        properties.insert(u"playbackController"_s, QVariant::fromValue(song.get()));
        properties.insert(u"backgroundMusicController"_s, QVariant::fromValue(&background));
        properties.insert(u"interfaceSoundController"_s, QVariant::fromValue(&sounds));
    }
    std::unique_ptr<QObject> object(component.createWithInitialProperties(properties));
    auto *root = qobject_cast<QQuickItem *>(object.get());
    if (!root) {
        qWarning() << component.errors();
        return 3;
    }
    QJsonObject lastSettingsSample;
    QJsonArray settingsResourceSamples;
    auto *settingsScene =
            qobject_cast<rhi::SceneItem *>(yalahoo::review::findVisual(root, u"nativeScene"_s));
    if (settingsVerification && settingsScene) {
        settingsScene->setTelemetryEnabled(true);
        QObject::connect(settingsScene, &rhi::SceneItem::frameSubmitted, root,
                         [&](const QJsonObject &sample) {
                             lastSettingsSample = sample;
                         });
    }
    QQuickRenderControl control;
    QQuickWindow window(OffscreenSurface::Enabled ? &control : nullptr);
    verification::prepareWindow(window, true);
    OffscreenSurface surface(window, control);
    root->setParentItem(window.contentItem());
    const auto deviceCaptured = std::make_shared<std::atomic_bool>(false);
    QObject::connect(
            &window, &QQuickWindow::afterRendering, &window,
            [&window, &control, deviceCaptured] {
                return reviewLibraryObserveFrame(deviceCaptured, control, window);
            },
            Qt::DirectConnection);
    input.attach(&window);
    QSignalSpy rendered(&surface, &OffscreenSurface::frameRendered);

    const auto nativeFrames = std::make_shared<NativeFrames>();
    QObject::connect(
            &window, &QQuickWindow::afterSynchronizing, &window,
            [nativeFrames] {
                ++nativeFrames->synchronized;
            },
            Qt::DirectConnection);
    QObject::connect(
            &window, &QQuickWindow::frameSwapped, &window,
            [nativeFrames] {
                nativeFrames->presented.store(nativeFrames->synchronized.load());
            },
            Qt::DirectConnection);
    QSignalSpy playback(&library, &LibrarySession::playbackChanged);
    const auto render = [&] {
        return reviewLibraryRender(nativeFrames, surface, rendered);
    };
    const auto renderSettingsTarget = [&] {
        return renderSettingsFrame(settingsScene, scene, lastSettingsSample, render, surface);
    };
    surface.show();
    motion.clock()->setActive(true);
    motion.clock()->advance(0);
    panels.setReducedMotion(true);
    motion.setReducedMotion(true);
    QJsonArray checks;
    QJsonObject context;
    bool passed = true;
    const auto check = [&](const QString &name, bool okay) {
        auto entry = context;
        entry.insert(u"check"_s, name);
        entry.insert(u"passed"_s, okay);
        checks.append(entry);
        if (!okay)
            qWarning() << "Library check failed:" << name;
        passed = okay && passed;
    };
    if (song) {
        window.resize(1280, 720);
        root->setSize({ 1280, 720 });
        render();
        auto *button = yalahoo::review::findVisual(window.contentItem(), u"playButton"_s);
        if (!button)
            return 4;
        QTest::mouseClick(
                &window, Qt::LeftButton, Qt::NoModifier,
                button->mapToScene({ button->width() / 2, button->height() / 2 }).toPoint());
        QElapsedTimer loaded;
        loaded.start();
        while (!song->playing() && loaded.elapsed() < MediaDeadlineMilliseconds)
            QTest::qWait(EventPollMilliseconds);
        check(u"reference-license-resource"_s,
              QFile::exists(u":/assets/licenses/qt-playback-reference-NOTICE.md"_s));
        check(u"actual-play-button"_s, song->playing() && song->trackId() == "track-0"_L1);
        root->forceActiveFocus(Qt::OtherFocusReason);
        QTest::keyClick(&window, Qt::Key_Space);
        check(u"space-pauses"_s, !song->requested());
        QTest::keyClick(&window, Qt::Key_Space);
        QElapsedTimer resumed;
        resumed.start();
        while (!song->playing() && resumed.elapsed() < FrameDeadlineMilliseconds)
            QTest::qWait(EventPollMilliseconds);
        check(u"space-resumes"_s, song->playing());
        playback.clear();
    }
    const QList<QSize> sizes = representative
            ? QList<QSize>{ { 1280, 720 }, { 390, 844 } }
            : QList<QSize>{ { 1920, 1080 }, { 1280, 720 },  { 995, 858 },
                            { 960, 900 },   { 930, 900 },   { 390, 844 },
                            { 320, 720 },   { 2560, 1080 }, { 3440, 1440 } };
    const QList<double> dprs = OffscreenSurface::Enabled
            ? (representative ? QList<double>{ 1, 2 } : QList<double>{ 1, 1.5, 2 })
            : QList<double>{ window.devicePixelRatio() };
    bool settingsInteractions = false, searchInteractions = false, progressInteractions = false;
    for (double dpr : dprs) {
        if (OffscreenSurface::Enabled)
            surface.setDevicePixelRatio(dpr);
        for (auto size : sizes) {
            root->setSize(size);
            window.resize(size);
            scene.layout()->setViewport(size.width(), size.height());
            for (bool night : { false, true }) {
                context = { { u"width"_s, size.width() },
                            { u"height"_s, size.height() },
                            { u"dpr"_s, dpr },
                            { u"night"_s, night } };
                scene.lighting()->setNight(night);
                if (settingsVerification) {
                    panels.open(u"settings"_s);
                    check(u"render-settings"_s, render());
                    auto *settingsPanel =
                            yalahoo::review::findVisual(window.contentItem(), u"settingsPanel"_s);
                    auto *pane = yalahoo::review::findVisual(window.contentItem(),
                                                             u"libraryPanelSurface"_s);
                    check(u"source-settings-panel"_s,
                          settingsPanel && pane
                                  && std::abs(pane->width()
                                              - std::min(
                                                      650.,
                                                      size.width()
                                                              - (size.width() <= 700 ? 24. : 56.)))
                                          <= 1);
                    auto *query = yalahoo::review::findVisual(window.contentItem(),
                                                              u"introductionMetadataQuery"_s);
                    check(u"offline-query-disabled"_s, query && !query->isEnabled());
                    auto *metadataSettings = settingsPanel
                            ? yalahoo::review::findVisual(settingsPanel, u"metadataSettings"_s)
                            : nullptr;
                    check(u"metadata-settings-section"_s, metadataSettings != nullptr);
                    if (metadataSettings)
                        metadataSettings->setProperty("expanded", true);
                    check(u"expanded-settings-render"_s, render());
                    auto *proxySwitch = yalahoo::review::findVisual(window.contentItem(),
                                                                    u"settingsProxyEnabled"_s);
                    auto *proxyMode = yalahoo::review::findVisual(window.contentItem(),
                                                                  u"settingsProxyMode"_s);
                    auto *proxyAddress = yalahoo::review::findVisual(window.contentItem(),
                                                                     u"settingsMetadataProxy"_s);
                    check(u"proxy-default-enabled"_s,
                          proxySwitch && proxySwitch->property("checked").toBool());
                    check(u"proxy-default-system"_s,
                          proxyMode && proxyMode->property("selected").toString() == "system"_L1);
                    check(u"manual-proxy-hidden-by-default"_s,
                          proxyAddress && !proxyAddress->isVisible());
                    auto *scroll = settingsPanel
                            ? settingsPanel->property("contentItem").value<QQuickItem *>()
                            : nullptr;
                    check(u"settings-scrollable"_s,
                          scroll
                                  && scroll->property("contentHeight").toDouble()
                                          > scroll->height());
                    if (scroll)
                        scroll->setProperty("contentY",
                                            std::max(0.,
                                                     scroll->property("contentHeight").toDouble()
                                                             - scroll->height()));
                    check(u"settings-bottom-render"_s, render());
                    auto *contact = yalahoo::review::findVisual(window.contentItem(),
                                                                u"settingsMetadataContact"_s);
                    check(u"configuration-reachable"_s,
                          settingsPanel && contact
                                  && settingsPanel->contains(settingsPanel->mapFromItem(
                                          contact,
                                          { contact->width() / 2, contact->height() / 2 })));
                    if (dpr == dprs.first() && (size.width() == 1280 || size.width() == 390))
                        check(u"settings-bottom-own-image"_s,
                              surface.grab().save(
                                      output
                                      + u"/settings-bottom-%1-%2.png"_s.arg(size.width())
                                                .arg(night)));
                    if (scroll)
                        scroll->setProperty("contentY", 0);
                    check(u"settings-top-render"_s, render());
                    if (dpr == dprs.first() && (size.width() == 1280 || size.width() == 390))
                        check(u"settings-top-own-image"_s,
                              surface.grab().save(
                                      output
                                      + u"/settings-top-%1-%2.png"_s.arg(size.width()).arg(night)));
                    if (dpr == dprs.first() && size == QSize(1280, 720) && !night) {
                        settingsInteractions = true;
                        verifySettingsInputs(check, renderSettingsTarget, lastSettingsSample,
                                             settingsScene, settingsResourceSamples, window, scroll,
                                             render, settings, motion, scene, surface, output,
                                             injected, library, query, snapshot);
                    }
                    QPointer<QQuickItem> oldPanel = settingsPanel;
                    QTest::keyClick(&window, Qt::Key_Escape);
                    check(u"escape-settings"_s,
                          render() && !panels.surface()->shown() && input.enabled() && !oldPanel);
                    panels.open(u"settings"_s);
                    check(u"reverse-open-settings"_s,
                          render()
                                  && yalahoo::review::findVisual(window.contentItem(),
                                                                 u"settingsPanel"_s));
                    panels.close();
                    check(u"settings-close"_s, render() && !panels.surface()->shown());
                    continue;
                }
                panels.open(u"search"_s);
                check(u"render-search"_s, render());
                auto *pane =
                        yalahoo::review::findVisual(window.contentItem(), u"libraryPanelSurface"_s);
                auto *query =
                        yalahoo::review::findVisual(window.contentItem(), u"librarySearchInput"_s);
                auto *results = yalahoo::review::findVisual(window.contentItem(),
                                                            u"librarySearchResults"_s);
                if (!pane || !query || !results)
                    return 4;
                auto *genreFilter =
                        yalahoo::review::findVisual(window.contentItem(), u"genreFilter-h"_s);
                auto *genreCaption = genreFilter
                        ? genreFilter->property("contentItem").value<QQuickItem *>()
                        : nullptr;
                check(u"genre-label-is-literal-text"_s,
                      genreCaption
                              && genreCaption->property("text").toString()
                                      == u"<b>\u6d41\u6d3e\u4e8c</b>"_s
                              && genreCaption->property("textFormat").toInt() == 0);
                check(u"source-panel-width"_s,
                      std::abs(pane->width()
                               - std::min(650., size.width() - (size.width() <= 700 ? 24. : 56.)))
                              <= 1);
                auto *sceneItem = qobject_cast<rhi::SceneItem *>(
                        yalahoo::review::findVisual(window.contentItem(), u"nativeScene"_s));
                check(u"scene-error-free"_s, sceneItem && sceneItem->errorText().isEmpty());
                check(u"panel-descriptor"_s,
                      sceneItem && sceneItem->panelLayerSize() == size
                              && sceneItem->input().panelLayerSize
                                      == size * window.effectiveDevicePixelRatio());
                if (dpr == dprs.first() && size == QSize(1280, 720) && !night) {
                    searchInteractions = true;
                    QPointer<QQuickItem> first = yalahoo::review::findVisual(
                            window.contentItem(), u"librarySearchResult-0"_s);
                    check(u"search-first-row"_s,
                          first && first->property("trackId").toString().isEmpty());
                    const QPoint press = first
                            ? first->mapToScene(QPointF(first->width() / 2, first->height() / 2))
                                      .toPoint()
                            : QPoint();
                    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, press);
                    check(u"search-row-pressed"_s, first && first->property("pressed").toBool());
                    query->setProperty("text", u"Song"_s);
                    check(u"search-query-ready"_s, waitForSearch(*library.searchResults()));
                    render();
                    check(u"search-row-reused"_s,
                          first
                                  && first
                                          == yalahoo::review::findVisual(window.contentItem(),
                                                                         u"librarySearchResult-0"_s)
                                  && first->property("trackId").toString() == "track-0"_L1);
                    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, press);
                    check(u"search-replaced-press-canceled"_s,
                          render() && panels.surface()->shown() && presentation.archiveEnabled());
                    check(u"large-results"_s, library.searchResults()->rowCount() == 4000);
                    int live = 0;
                    for (auto *item : results->childItems())
                        live += item->childItems().size();
                    check(u"large-result-virtualization"_s, live < 100);
                    query->forceActiveFocus();
                    QTest::keyClick(&window, Qt::Key_Down);
                    for (int i = 0; i < 6; ++i)
                        QTest::keyClick(&window, Qt::Key_Down);
                    check(u"search-keyboard-row-selected"_s,
                          results->property("currentIndex").toInt() > 0);
                    query->forceActiveFocus();
                    query->setProperty("text", u"Song 199"_s);
                    check(u"new-query-discards-old-keyboard-target"_s,
                          render() && results->property("currentIndex").toInt() <= 0);
                    query->setProperty("text", u""_s);
                    query->forceActiveFocus();
                }
                check(u"search-focus-and-modal"_s, query->hasActiveFocus() && !input.enabled());
                for (const auto character : "Song 199"_ba)
                    QTest::keyClick(&window, character);
                check(u"continuous-input"_s,
                      waitForSearch(*library.searchResults())
                              && library.searchResults()->rowCount() == 20);
                if (song)
                    check(u"search-space-keeps-intent"_s, song->requested());
                query->setProperty("text", u"unmatched"_s);
                check(u"empty-results"_s,
                      waitForSearch(*library.searchResults())
                              && library.searchResults()->rowCount() == 0);
                query->setProperty("text", u""_s);
                check(u"empty-query-albums"_s,
                      waitForSearch(*library.searchResults())
                              && library.searchResults()->rowCount() == 20);
                int delegates = 0;
                for (auto *item : results->childItems())
                    delegates += item->childItems().size();
                check(u"virtual-search-list"_s, delegates < 100);
                render();
                if ((size == QSize(1280, 720) || size == QSize(390, 844)) && dpr == dprs.first())
                    check(u"own-image"_s,
                          surface.grab().save(
                                  output + u"/search-%1-%2.png"_s.arg(size.width()).arg(night)));
                QPointer<QQuickItem> backdrop =
                        yalahoo::review::findVisual(window.contentItem(), u"panelBackdropLoader"_s);
                panels.close();
                check(u"close-search"_s, render() && !panels.surface()->shown() && input.enabled());
                if (dpr == dprs.first() && size == QSize(1280, 720) && !night)
                    qInfo() << "Closed backdrop" << bool(backdrop)
                            << (backdrop ? backdrop->property("active") : QVariant())
                            << (backdrop ? backdrop->childItems().size() : -1)
                            << (sceneItem ? sceneItem->input().panelLayerSize : QSize());
                check(u"closed-backdrop-released"_s,
                      backdrop && !backdrop->property("active").toBool()
                              && backdrop->childItems().isEmpty() && sceneItem
                              && sceneItem->input().panelLayerSize.isEmpty());
                panels.open(u"library"_s);
                check(u"render-library"_s, render());
                if (size == QSize(1280, 720) && dpr == dprs.first())
                    check(u"own-library-image"_s,
                          surface.grab().save(output + u"/library-%1.png"_s.arg(night)));
                panels.close();
                render();
                if (song) {
                    panels.open(u"playback"_s);
                    check(u"playback-panel-render"_s, render());
                    for (const auto &name : { "bgmEnabled", "bgmVolume", "interfaceSoundEnabled",
                                              "interfaceSoundVolume" })
                        check(u"audio-setting-%1"_s.arg(name),
                              yalahoo::review::findVisual(window.contentItem(),
                                                          QString::fromUtf8(name))
                                      != nullptr);
                    check(u"audio-independent-values"_s,
                          background.volume() == .18 && sounds.volume() == .22
                                  && song->volume() == double(.01f) && !sounds.available());
                    auto *outputs = yalahoo::review::findVisual(window.contentItem(),
                                                                u"songOutputDevices"_s);
                    check(u"output-device-control"_s, outputs && outputs->isVisible());
                    auto *settings = yalahoo::review::findVisual(window.contentItem(),
                                                                 u"audioSettingsPanel"_s);
                    auto *scroll = settings
                            ? settings->property("contentItem").value<QQuickItem *>()
                            : nullptr;
                    const double previousY = scroll ? scroll->property("contentY").toDouble() : 0;
                    if (scroll)
                        scroll->setProperty("contentY",
                                            std::max(0.,
                                                     scroll->property("contentHeight").toDouble()
                                                             - scroll->height()));
                    check(u"audio-settings-scroll-render"_s, scroll && render());
                    check(u"audio-output-reachable"_s,
                          settings && outputs
                                  && settings->contains(settings->mapFromItem(
                                          outputs,
                                          QPointF(outputs->width() / 2, outputs->height() / 2))));
                    if (dpr == dprs.first() && (size.width() == 1280 || size.width() == 390))
                        check(u"audio-output-own-image"_s,
                              surface.grab().save(
                                      output
                                      + u"/audio-output-%1-%2.png"_s.arg(size.width()).arg(night)));
                    if (scroll)
                        scroll->setProperty("contentY", previousY);
                    check(u"audio-settings-top-render"_s, render());
                    auto *progress =
                            yalahoo::review::findVisual(window.contentItem(), u"songProgress"_s);
                    check(u"playback-progress-control"_s,
                          progress && progress->isVisible() && progress->isEnabled());
                    if (dpr == dprs.first() && (size.width() == 1280 || size.width() == 390))
                        check(u"playback-own-image"_s,
                              surface.grab().save(
                                      output
                                      + u"/playback-%1-%2.png"_s.arg(size.width()).arg(night)));
                    if (dpr == dprs.first() && size == QSize(1280, 720) && !night && progress) {
                        progressInteractions = true;
                        song->pause();
                        const auto before = song->position();
                        progress->forceActiveFocus();
                        QTest::keyClick(&window, Qt::Key_Right);
                        check(u"progress-key-seeks"_s,
                              song->position() > before && !song->requested());
                        QTest::keyClick(&window, Qt::Key_Space);
                        check(u"slider-space-owned"_s, !song->requested());
                        const auto point = [&](double ratio) {
                            return progress
                                    ->mapToScene(
                                            { progress->width() * ratio, progress->height() / 2 })
                                    .toPoint();
                        };
                        QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, point(.4));
                        QTest::mouseMove(&window, point(.5));
                        check(u"progress-pointer-preview"_s,
                              song->dragging() && song->position() > 5000 && !song->requested());
                        QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, point(.5));
                        check(u"progress-pointer-commit"_s,
                              !song->dragging() && song->position() > 5000 && !song->requested());
                        song->seek(1000);
                        song->toggle();
                    }
                    panels.close();
                    render();
                }
            }
        }
    }
    if (!settingsVerification) {
        context = { { u"scenario"_s, u"many-genres"_s }, { u"dpr"_s, dprs.last() } };
        LibrarySnapshot large = snapshot;
        for (int i = 0; i < 1024; ++i) {
            const auto id = u"genre-%1"_s.arg(i);
            large.genres.append({ id, u"\u5206\u7c7b %1"_s.arg(i), { }, { }, { } });
            Album album;
            album.id = u"extra-album-%1"_s.arg(i);
            album.title = album.id;
            album.genreId = id;
            large.albums.append(album);
        }
        library.setSnapshot(large);
        selection.setColumns(library.albums()->order().columns, library.snapshot());
        panels.open(u"search"_s);
        check(u"large-genres-render"_s, render());
        auto *genres = yalahoo::review::findVisual(window.contentItem(), u"libraryGenreFilters"_s);
        const auto countRows = [&] {
            int rows = 0;
            QList<QQuickItem *> pending;
            if (genres)
                pending.append(genres);
            for (qsizetype i = 0; i < pending.size(); ++i) {
                rows += pending[i]->objectName() == "genreFilterRow"_L1;
                pending.append(pending[i]->childItems());
            }
            return rows;
        };
        const int liveGenreRows = countRows();
        check(u"large-genres-virtualization"_s,
              genres && liveGenreRows > 0 && liveGenreRows < 100
                      && library.genres().size() == 1026);
        if (genres)
            QMetaObject::invokeMethod(genres, "positionViewAtEnd");
        check(u"large-genres-scroll"_s, render());
        auto *lastGenre =
                yalahoo::review::findVisual(window.contentItem(), u"genreFilter-genre-1023"_s);
        check(u"large-genres-last-identity"_s,
              lastGenre && lastGenre->isVisible()
                      && lastGenre->property("text").toString() == u"\u5206\u7c7b 1023"_s
                      && countRows() > 0 && countRows() < 100);
        if (lastGenre) {
            QTest::mouseClick(
                    &window, Qt::LeftButton, Qt::NoModifier,
                    lastGenre->mapToScene(QPointF(lastGenre->width() / 2, lastGenre->height() / 2))
                            .toPoint());
        }
        check(u"large-genres-selection"_s,
              waitForSearch(*library.searchResults()) && library.searchGenreId() == u"genre-1023"_s
                      && library.searchResults()->rowCount() == 1
                      && library.searchResults()->resultAt(0)[u"albumId"_s].toString()
                              == u"extra-album-1023"_s);
        panels.close();
        library.setSnapshot(snapshot);
        selection.setColumns(library.albums()->order().columns, library.snapshot());
        presentation.reset();
        context = { { u"scenario"_s, u"track-reveal"_s },
                    { u"width"_s, 1280 },
                    { u"height"_s, 720 },
                    { u"dpr"_s, dprs.last() } };
        window.resize(1280, 720);
        root->setSize({ 1280, 720 });
        scene.layout()->setViewport(1280, 720);
        render();
        motion.setReducedMotion(false);
        QSignalSpy reveal(&focus, &TrackFocusController::revealRequested);
        QSignalSpy focused(&focus, &TrackFocusController::focusRequested);
        check(u"request-last-track"_s, focus.request(u"album-12"_s, u"track-199"_s));
        for (int frame = 0; frame < 2400 && focused.isEmpty(); ++frame) {
            motion.clock()->advance(1. / 60);
            if (frame % 4 == 0)
                render();
        }
        render();
        check(u"detail-track-scroll-and-focus"_s,
              !focused.isEmpty() && reveal.size() == 1
                      && library.navigationAlbumId() == "album-12"_L1
                      && library.navigationTrackId() == "track-199"_L1);
        qInfo() << "Focus actual"
                << (window.activeFocusItem() ? window.activeFocusItem()->metaObject()->className()
                                             : "null")
                << (window.activeFocusItem() ? window.activeFocusItem()->objectName() : QString())
                << focus.active() << focus.revision() << focus.scrollY();
        check(u"actual-track-focus"_s,
              window.activeFocusItem()
                      && window.activeFocusItem()->property("trackId").toString()
                              == "track-199"_L1);
        auto *trackList = yalahoo::review::findVisual(window.contentItem(), u"detailTrackList"_s);
        auto *trackView = trackList
                ? qobject_cast<QQuickItem *>(trackList->property("currentView").value<QObject *>())
                : nullptr;
        auto *focusedRow = window.activeFocusItem();
        const double lastBottom = focusedRow && trackView
                ? focusedRow->mapToItem(trackView, { 0, focusedRow->height() }).y()
                : -1;
        qInfo() << "Track geometry" << trackView << lastBottom
                << (trackView ? trackView->property("contentY") : QVariant())
                << (trackView ? trackView->property("contentHeight") : QVariant())
                << (trackView ? trackView->property("originY") : QVariant());
        check(u"last-row-clamped-to-content"_s,
              trackView && std::abs(lastBottom - trackView->height()) <= 1);
        const auto layout = presentation.layoutRevision();
        if (trackList && trackList->parentItem())
            trackList->parentItem()->setProperty("activeTab", 1);
        check(u"reuse-open-album"_s, focus.request(u"album-12"_s, u"track-100"_s));
        render();
        for (int frame = 0; frame < 240
             && (!window.activeFocusItem()
                 || window.activeFocusItem()->property("trackId").toString() != "track-100"_L1);
             ++frame) {
            motion.clock()->advance(1. / 60);
            if (frame % 4 == 0)
                render();
        }
        render();
        trackView = trackList
                ? qobject_cast<QQuickItem *>(trackList->property("currentView").value<QObject *>())
                : nullptr;
        auto *middleRow = window.activeFocusItem();
        const double center = middleRow && trackView
                ? middleRow
                          ->mapToItem(
                                  trackView,
                                  { 0,
                                    middleRow->property("trackTop").toDouble()
                                            + middleRow->property("trackHeight").toDouble() / 2 })
                          .y()
                : -1;
        qInfo() << "Middle focus" << (middleRow ? middleRow->property("trackId") : QVariant())
                << center << (trackView ? trackView->height() : -1)
                << (trackList ? trackList->property("tabHeight") : QVariant())
                << (middleRow ? middleRow->property("trackTop") : QVariant())
                << (middleRow ? middleRow->property("trackHeight") : QVariant())
                << (trackView ? trackView->property("contentY") : QVariant())
                << (trackView ? trackView->property("originY") : QVariant()) << focus.scrollY();
        check(u"disc-heading-track-centered"_s,
              middleRow && middleRow->property("trackId").toString() == "track-100"_L1 && trackList
                      && trackView
                      && std::abs(center
                                  - (trackView->height()
                                     + trackList->property("tabHeight").toDouble())
                                          / 2)
                              <= 1);
        check(u"no-reextract"_s,
              presentation.layoutRevision() == layout && presentation.detailEnabled());
        auto *tracks = yalahoo::review::findVisual(window.contentItem(), u"detailTrackList"_s);
        if (tracks)
            QMetaObject::invokeMethod(tracks, "cancelReveal");
        check(u"user-cancel"_s, !focus.active());
        check(u"playback-unchanged"_s,
              playback.isEmpty() && library.playbackTrackId() == "track-0"_L1);
        if (song) {
            check(u"playback-seekable"_s, song->seekable());
            song->pause();
            song->beginSeek();
            song->previewSeek(1000);
            song->commitSeek();

            check(u"navigation-keeps-real-source"_s,
                  song->trackId() == "track-0"_L1 && song->source() == audioSource);
            song->stop();
            check(u"playback-stop"_s, !song->requested() && song->source().isEmpty());
        }
    }
    if (settingsVerification) {
        context = { { u"scenario"_s, u"production-visual-reload"_s } };
        auto appProperties = properties;
        appProperties.insert(u"flags"_s, int(verification::windowFlags()));
        QQmlComponent appComponent(&engine, QUrl(u"qrc:/qt/qml/Yalahoo/Music/App.qml"_s));
        const bool appLoaded = QTest::qWaitFor([&] {
            return appComponent.status() != QQmlComponent::Loading;
        });
        if (!appLoaded || appComponent.isError())
            qWarning() << appComponent.errors();
        std::unique_ptr<QObject> appRoot(appComponent.createWithInitialProperties(appProperties));
        auto *appWindow = qobject_cast<QQuickWindow *>(appRoot.get());
        check(u"production-app-created"_s, appWindow != nullptr);
        if (appWindow) {
            verification::prepareWindow(*appWindow);
            const auto source = song->source();
            const auto track = song->trackId();
            const auto requested = song->requested();
            auto *player = song.get();
            const auto values = settings.values();
            {
                QPointer<QQuickItem> oldScene =
                        yalahoo::review::findVisual(appWindow->contentItem(), u"nativeScene"_s);
                settings.reload();
                QElapsedTimer timer;
                timer.start();
                while ((!oldScene.isNull()
                        || !yalahoo::review::findVisual(appWindow->contentItem(), u"nativeScene"_s))
                       && timer.elapsed() < InputDeadlineMilliseconds)
                    QTest::qWait(EventPollMilliseconds);
                check(u"reload-rebuilds-visual"_s,
                      oldScene.isNull()
                              && yalahoo::review::findVisual(appWindow->contentItem(),
                                                             u"nativeScene"_s));
                check(u"reload-preserves-media-quality"_s,
                      song.get() == player && song->source() == source && song->trackId() == track
                              && song->requested() == requested && settings.values() == values);
            }
            check(u"hidden-app-policy"_s,
                  !appWindow->isVisible() && verification::windowPolicySatisfied(*appWindow));
        }
        appRoot.reset();
        check(u"default-offline-capabilities"_s,
              !library.providerCapabilities()[u"networkEnabled"_s].toBool());
        song->stop();
    }
    check(u"representative-interactions-executed"_s,
          settingsVerification
                  ? settingsInteractions
                  : searchInteractions && (!playbackVerification || progressInteractions));
    check(u"qml-warnings"_s, warnings.isEmpty());
    const auto policy = verification::windowPolicy(window);
    check(u"window-policy"_s, policy.value(u"passed"_s).toBool());
    QFile executable(QCoreApplication::applicationFilePath());
    QCryptographicHash executableHash(QCryptographicHash::Sha256);
    const bool hashed = executable.open(QIODevice::ReadOnly) && executableHash.addData(&executable);
    QJsonObject report{
        { u"passed"_s, passed },
        { u"representative"_s, representative },
        { u"interactionDpr"_s, dprs.first() },
        { u"settingsInteractions"_s, settingsInteractions },
        { u"searchInteractions"_s, searchInteractions },
        { u"progressInteractions"_s, progressInteractions },
        { u"checks"_s, checks },
        { u"windowPolicy"_s, policy },
        { u"backend"_s,
          window.property("libraryChecksDevice").toMap().value(u"backend"_s).toString() },
        { u"gpu"_s, window.property("libraryChecksDevice").toMap().value(u"gpu"_s).toString() },
        { u"os"_s, QSysInfo::prettyProductName() },
        { u"architecture"_s, QSysInfo::currentCpuArchitecture() },
        { u"qt"_s, QString::fromUtf8(qVersion()) },
        { u"binarySha256"_s,
          hashed ? QString::fromUtf8(executableHash.result().toHex()) : QString() },
        { u"fixtureSha256"_s,
          QString::fromUtf8(QCryptographicHash::hash(
                                    QJsonDocument(toJson(snapshot)).toJson(QJsonDocument::Compact),
                                    QCryptographicHash::Sha256)
                                    .toHex()) },
        { u"offscreen"_s, OffscreenSurface::Enabled },
        { u"settingsResourceSamples"_s, settingsResourceSamples },
        { u"scope"_s,
          u"source geometry, normal output, Qt focus/input, virtual delegates and track reveal; no image scoring or display latency claims"_s }
    };
    if (song)
        report.insert(
                u"audio"_s,
                QJsonObject{
                        { u"available"_s, song->outputAvailable() },
                        { u"device"_s, song->outputName() },
                        { u"deviceCount"_s, song->devices().size() - 1 },
                        { u"scope"_s,
                          u"actual public Qt playback and controls; no physical unplug or listening claim"_s } });
    if (!yalahoo::writeJsonFile(output + "/report.json"_L1, QJsonDocument(report)))
        return 5;
    object.reset();
    surface.hide();
    return passed ? 0 : 6;
}
