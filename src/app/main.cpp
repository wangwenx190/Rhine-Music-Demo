#include "CoverCache.h"
#include <QScopeGuard>
#include "AppIdentity.h"
#include "Logging.h"
#include "AssetCatalog.h"
#include "LibrarySession.h"
#include "PlaybackController.h"
#include "BackgroundMusicController.h"
#include "InterfaceSoundController.h"
#include "LibraryStore.h"
#include "PresentationController.h"
#include "DetailModel.h"
#include "TrackFocusController.h"
#include "PanelController.h"
#include "DetailPorts.h"
#include "MotionController.h"
#include "OpeningController.h"
#include "StartupCoordinator.h"
#include "OfflineMetadataProvider.h"
#include "PreferencesStore.h"
#include "SettingsController.h"
#include "DemoLibrary.h"
#include "SceneAdapter.h"
#include "VerificationWindow.h"
#include "GraphicsBackend.h"
#include "DisplayOutput.h"
#include "DeploymentProbe.h"
#ifdef Q_OS_MACOS
#  include "ApplicationMenu.h"
#endif
#include "BrowseReview.h"
#include "NavigationReview.h"
#include "DetailReview.h"
#include "OverviewController.h"
#include "OverviewReview.h"

#include "InputRouter.h"
#include "RulerController.h"
#include "Localization.h"
#ifdef YALAHOO_REVIEW
#  include "SceneReview.h"
#  include "IntegrationReview.h"
#  include "MaterialReview.h"
#endif

#include <QAudioDevice>
#include <QAudioOutput>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QMutex>
#include <QMutexLocker>
#include <QMouseEvent>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRawFont>
#include <QSaveFile>
#include <QSGRendererInterface>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <utility>

using namespace Qt::StringLiterals;

namespace {
QMutex diagnosticsMutex;
QStringList diagnostics;
QtMessageHandler diagnosticSink = nullptr;
void recordDiagnostic(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    const QMutexLocker lock(&diagnosticsMutex);
    if (diagnostics.size() == 1024)
        diagnostics.removeFirst();
    diagnostics.append(message);
    if (diagnosticSink)
        diagnosticSink(type, context, message);
}
}

int main(int argc, char **argv)
{
    bool collectDiagnostics = false;
    for (int i = 1; i < argc; ++i) {
        if (QByteArray(argv[i]).startsWith("--verify-"_ba)) {
            collectDiagnostics = true;
            yalahoo::verification::prepareApplication();
        }
    }
    yalahoo::initializeIdentity();
    const yalahoo::LoggingSession logging;
    // Install before QGuiApplication can start threads that emit Qt messages.
    if (collectDiagnostics)
        diagnosticSink = qInstallMessageHandler(recordDiagnostic);
    const auto diagnosticRestore = qScopeGuard([] {
        if (diagnosticSink)
            qInstallMessageHandler(diagnosticSink);
    });
    qCInfo(yalahoo::logApp) << "Application starting" << "Qt=" << qVersion()
                            << "platform=" << QSysInfo::prettyProductName()
                            << "architecture=" << QSysInfo::currentCpuArchitecture();
    if (!yalahoo::selectGraphicsBackend())
        return 22;
    QGuiApplication app(argc, argv);
    auto globalFont = QGuiApplication::font();
    globalFont.setStyleStrategy(
            QFont::StyleStrategy(QFont::PreferAntialias | QFont::PreferQuality));
    globalFont.setHintingPreference(QFont::PreferNoHinting);
    QGuiApplication::setFont(globalFont);
    yalahoo::DisplayOutput displayOutput;
    const yalahoo::Localization localization;
    if (!yalahoo::probeOptionalGraphicsBackend())
        return 22;
    QIcon applicationIcon;
    for (const int size : { 16, 24, 32, 48, 64, 128, 256, 512, 1024 }) {
        applicationIcon.addFile(u":/assets/icons/yalahoo/png/yalahoo-%1.png"_s.arg(size),
                                QSize(size, size));
    }
    app.setWindowIcon(applicationIcon);
    const auto coverShutdown = qScopeGuard([] {
        yalahoo::CoverCache::finishDecoders();
    });
    if (!yalahoo::initializeAssetFonts()) {
        qCCritical(yalahoo::logApp) << "Embedded MiSans font registration failed";
        return 6;
    }
    QCommandLineParser parser;
    parser.setApplicationDescription(
            QCoreApplication::translate("CommandLine", "yalahOO!!! native music archive"));
    parser.addHelpOption();
    parser.addOption({ u"verify-app"_s,
                       QCoreApplication::translate(
                               "CommandLine", "Verify hidden product QML creation, then exit") });
    parser.addOption(
            { u"demo"_s,
              QCoreApplication::translate(
                      "CommandLine",
                      "Use anonymous albums without reading saved data or starting audio") });
    parser.addOption({ u"lighting-lab"_s,
                       QCoreApplication::translate(
                               "CommandLine",
                               "Open lighting/motion controls with the anonymous demonstration") });
    parser.addOption({ u"verify-deployment"_s,
                       QCoreApplication::translate(
                               "CommandLine",
                               "Verify hidden product resources and media loading, then exit"),
                       QCoreApplication::translate("CommandLine", "report") });
    parser.addOption(
            { u"verify-overview"_s,
              QCoreApplication::translate("CommandLine", "Verify production overview and exit"),
              QCoreApplication::translate("CommandLine", "directory") });
    parser.addOption({ u"verify-detail"_s,
                       QCoreApplication::translate("CommandLine",
                                                   "Verify production detail motion and exit"),
                       QCoreApplication::translate("CommandLine", "directory") });
    parser.addOption(
            { u"verify-navigation"_s,
              QCoreApplication::translate("CommandLine", "Verify production navigation and exit"),
              QCoreApplication::translate("CommandLine", "directory") });
    parser.addOption(
            { u"verify-browse"_s,
              QCoreApplication::translate("CommandLine", "Verify production browse UI and exit"),
              QCoreApplication::translate("CommandLine", "directory") });
    parser.addOption({ u"data-dir"_s,
                       QCoreApplication::translate("CommandLine", "Override the native data root"),
                       QCoreApplication::translate("CommandLine", "directory") });
    parser.addOption(
            { u"music-root"_s,
              QCoreApplication::translate(
                      "CommandLine",
                      "Scan a local absolute music directory (repeatable, replaces saved roots)"),
              QCoreApplication::translate("CommandLine", "directory") });
    parser.addOption(
            { u"import-legacy"_s,
              QCoreApplication::translate(
                      "CommandLine", "Read a legacy music-data-v3 directory into the native cache"),
              QCoreApplication::translate("CommandLine", "directory") });
    parser.addOption(
            { u"import-preferences"_s,
              QCoreApplication::translate("CommandLine",
                                          "Import an explicitly exported browser preferences JSON"),
              QCoreApplication::translate("CommandLine", "file") });
    parser.addOption(
            { u"rescan-library"_s,
              QCoreApplication::translate("CommandLine", "Rescan saved local music directories") });
    parser.addOption(
            { u"verify-runtime"_s,
              QCoreApplication::translate("CommandLine", "Write runtime evidence and exit"),
              QCoreApplication::translate("CommandLine", "report") });
    parser.addOption(
            { u"verify-library"_s,
              QCoreApplication::translate(
                      "CommandLine", "Load anonymous mixed library for runtime verification only"),
              QCoreApplication::translate("CommandLine", "fixture") });
    parser.addOption(
            { u"verify-filled"_s,
              QCoreApplication::translate(
                      "CommandLine",
                      "Exercise the production filled demonstration with verify-runtime") });
    parser.addOption(
            { u"verify-motion"_s,
              QCoreApplication::translate(
                      "CommandLine",
                      "Verify the production frame source, hide/restore and motion completion") });
    parser.addOption(
            { u"verify-shading"_s,
              QCoreApplication::translate(
                      "CommandLine",
                      "Exercise production material/shadow/AO rendering with verify-runtime") });
#ifdef YALAHOO_REVIEW
    parser.addOption({ u"verify-integration"_s,
                       QCoreApplication::translate("CommandLine",
                                                   "Measure RHI production integration and exit"),
                       QCoreApplication::translate("CommandLine", "directory") });
    parser.addOption({ u"integration-seconds"_s,
                       QCoreApplication::translate("CommandLine",
                                                   "Production sampling duration (12-120 seconds)"),
                       QCoreApplication::translate("CommandLine", "seconds"), u"12"_s });
    parser.addOption({ u"verify-materials"_s,
                       QCoreApplication::translate(
                               "CommandLine", "Verify source material state and native output"),
                       QCoreApplication::translate("CommandLine", "fixture") });
    parser.addOption({ u"material-report"_s,
                       QCoreApplication::translate("CommandLine", "Material source/state report"),
                       QCoreApplication::translate("CommandLine", "report") });
    parser.addOption(
            { u"verify-scene"_s,
              QCoreApplication::translate("CommandLine", "Run deterministic geometry fixtures"),
              QCoreApplication::translate("CommandLine", "fixture") });
    parser.addOption({ u"scene-report"_s,
                       QCoreApplication::translate("CommandLine", "Geometry review report"),
                       QCoreApplication::translate("CommandLine", "report") });
#endif
    parser.process(app);
    const bool demonstration = parser.isSet(u"demo"_s) || parser.isSet(u"lighting-lab"_s);
    if (demonstration) {
        for (const auto &option : parser.optionNames()) {
            if (option.startsWith(u"verify-"_s)
                || QStringList{ u"data-dir"_s, u"music-root"_s, u"import-legacy"_s,
                                u"import-preferences"_s, u"rescan-library"_s }
                           .contains(option)) {
                qCWarning(yalahoo::logApp,
                          "Demonstration cannot be combined with data or verification options");
                return 7;
            }
        }
    }
    const bool appVerification = parser.isSet(u"verify-app"_s)
            || parser.isSet(u"verify-deployment"_s) || parser.isSet(u"verify-browse"_s)
            || parser.isSet(u"verify-navigation"_s) || parser.isSet(u"verify-detail"_s)
            || parser.isSet(u"verify-overview"_s);
    if (parser.isSet(u"verify-filled"_s)
        && (!parser.isSet(u"verify-runtime"_s) || parser.isSet(u"verify-library"_s)
            || parser.isSet(u"verify-motion"_s)))
        return 7;
    if (parser.isSet(u"verify-shading"_s) && !parser.isSet(u"verify-runtime"_s))
        return 7;
    // Outlive the engine and its render-thread callbacks during teardown.
    std::atomic<int> frames{ 0 };
    std::atomic<int> api{ QSGRendererInterface::Unknown };
    yalahoo::OfflineMetadataProvider provider;
    yalahoo::LibrarySession library(&provider);
    yalahoo::BackgroundMusicController background;
    yalahoo::InterfaceSoundController sounds;
    yalahoo::PlaybackController playback(&library);
    playback.setBackgroundMusic(&background);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &playback, [&] {
        playback.stop();
        background.stop();
        sounds.stop();
    });
    if (parser.isSet(u"verify-library"_s)) {
        if (!parser.isSet(u"verify-runtime"_s))
            return 7;
        QFile input(parser.value(u"verify-library"_s));
        if (!input.open(QIODevice::ReadOnly))
            return 7;
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(input.readAll(), &parseError);
        yalahoo::LibrarySnapshot snapshot;
        QString error;
        if (parseError.error != QJsonParseError::NoError
            || !yalahoo::readLibrary(
                    document.object().value(u"libraries"_s).toObject().value(u"mixed"_s).toObject(),
                    snapshot, &error))
            return 7;
        library.setSnapshot(std::move(snapshot));
        library.search(u"Typography"_s);
        if (!library.snapshot()->albums.isEmpty())
            library.requestMetadata(library.snapshot()->albums.first().id, u"introduction"_s);
    }
    const bool verificationRun = parser.isSet(u"verify-runtime"_s) || appVerification
#ifdef YALAHOO_REVIEW
            || parser.isSet(u"verify-materials"_s) || parser.isSet(u"verify-scene"_s)
            || parser.isSet(u"verify-integration"_s)
#endif
            ;
    yalahoo::MotionController motion;
    yalahoo::OverviewController overview(&motion);
    yalahoo::SelectionController selection(&motion);
    yalahoo::SceneAdapter scene(motion.clock());
    yalahoo::OpeningController opening(&motion, &scene, &selection);
    yalahoo::StartupCoordinator startup(&library, &opening, motion.clock(),
                                        !verificationRun && !demonstration);
    yalahoo::InputRouter inputRouter(&selection);
    yalahoo::RulerController rulerController(&motion, &selection);
    rulerController.setInputRouter(&inputRouter);
    QObject::connect(motion.clock(), &yalahoo::FrameClock::frame, &inputRouter,
                     [&](double, double) {
                         inputRouter.flush();
                     });
    QObject::connect(&inputRouter, &yalahoo::InputRouter::inputReceived, &scene,
                     &yalahoo::SceneAdapter::recordInput);
    QObject::connect(&inputRouter, &yalahoo::InputRouter::navigationCommitted, &scene,
                     &yalahoo::SceneAdapter::recordNavigationCommit);
    QObject::connect(&rulerController, &yalahoo::RulerController::inputReceived, &scene,
                     &yalahoo::SceneAdapter::recordInput);
    QObject::connect(&motion, &yalahoo::MotionController::reducedMotionChanged, &scene, [&] {
        scene.setReducedMotion(motion.reducedMotion());
    });
    const auto updateApplicationVisibility = [&](Qt::ApplicationState state) {
        motion.clock()->setApplicationVisible(state != Qt::ApplicationHidden
                                              && state != Qt::ApplicationSuspended);
    };
    updateApplicationVisibility(app.applicationState());
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &motion,
                     updateApplicationVisibility);
    QJsonObject motionEvidence;
    double pausedTime = 0;
    quint64 pausedFrames = 0;
    bool awaitingResume = false;
    bool resumeDiscarded = true;
    int resumeSamples = 0;
    QJsonArray completionRevisions;
    std::unique_ptr<yalahoo::PreferencesStore> preferences;
    std::unique_ptr<yalahoo::LibraryStore> libraryStore;
    if (!verificationRun && !demonstration) {
        const QString dataRoot = parser.isSet(u"data-dir"_s)
                ? parser.value(u"data-dir"_s)
                : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (!QDir::isAbsolutePath(dataRoot)) {
            qCWarning(yalahoo::logApp, "--data-dir requires an absolute path");
            return 7;
        }
        preferences = std::make_unique<yalahoo::PreferencesStore>(dataRoot);
        const auto applyAudio = [&] {
            const auto audio = preferences->audio();
            playback.setVolume(audio.value(u"volume"_s).toDouble(.65));
            background.setVolume(audio.value(u"bgmVolume"_s).toDouble(.18));
            background.setEnabled(audio.value(u"bgm"_s).toBool(true));
            sounds.setVolume(audio.value(u"soundVolume"_s).toDouble(.22));
            sounds.setEnabled(audio.value(u"sound"_s).toBool(true));
        };
        applyAudio();
        QObject::connect(preferences.get(), &yalahoo::PreferencesStore::imported, &background,
                         applyAudio);
        const auto saveAudio = [&] {
            preferences->setAudio({ { u"volume"_s, playback.volume() },
                                    { u"bgm"_s, background.enabled() },
                                    { u"bgmVolume"_s, background.volume() },
                                    { u"sound"_s, sounds.enabled() },
                                    { u"soundVolume"_s, sounds.volume() } });
        };
        QObject::connect(&playback, &yalahoo::PlaybackController::volumeChanged, preferences.get(),
                         saveAudio);
        QObject::connect(&background, &yalahoo::BackgroundMusicController::settingsChanged,
                         preferences.get(), saveAudio);
        QObject::connect(&sounds, &yalahoo::InterfaceSoundController::settingsChanged,
                         preferences.get(), saveAudio);
        libraryStore = std::make_unique<yalahoo::LibraryStore>(dataRoot);
        QObject::connect(libraryStore.get(), &yalahoo::LibraryStore::snapshotReady, &startup,
                         &yalahoo::StartupCoordinator::receive);
        QObject::connect(libraryStore.get(), &yalahoo::LibraryStore::failed, &app,
                         [&](const QString &) {
                             if (!startup.statistics()[u"initialReady"_s].toBool())
                                 startup.receive(libraryStore->snapshot());
                         });
        if (parser.isSet(u"import-preferences"_s))
            if (!preferences->importJson(
                        QUrl::fromLocalFile(parser.value(u"import-preferences"_s))))
                return 7;
        if (parser.isSet(u"import-legacy"_s)
            && (parser.isSet(u"music-root"_s) || parser.isSet(u"rescan-library"_s))) {
            qCWarning(yalahoo::logApp, "--import-legacy cannot be combined with scan options");
            return 7;
        }
        libraryStore->load();
        if (parser.isSet(u"import-legacy"_s)) {
            if (!libraryStore->importLegacy(QUrl::fromLocalFile(parser.value(u"import-legacy"_s))))
                return 7;
        } else if (parser.isSet(u"music-root"_s)) {
            QList<QUrl> roots;
            for (const auto &path : parser.values(u"music-root"_s)) {
                if (!QDir::isAbsolutePath(path)) {
                    qCWarning(yalahoo::logApp, "--music-root requires absolute paths");
                    return 7;
                }
                roots.append(QUrl::fromLocalFile(path));
            }
            if (!libraryStore->setRoots(roots))
                return 7;
        } else if (parser.isSet(u"rescan-library"_s)) {
            const auto once = std::make_shared<QMetaObject::Connection>();
            *once = QObject::connect(libraryStore.get(), &yalahoo::LibraryStore::snapshotReady,
                                     libraryStore.get(),
                                     [store = libraryStore.get(), once](yalahoo::Snapshot) {
                                         QObject::disconnect(*once);
                                         store->rescan();
                                     });
        }
    }
    yalahoo::SettingsController settings(preferences.get(), &library, &motion, &scene, &selection);
    if (!verificationRun || parser.isSet(u"verify-filled"_s) || appVerification) {
        // Verification fixtures stay isolated from the user's persistent library.
        if (verificationRun || demonstration) {
            library.setSnapshot(yalahoo::demonstrationLibrary());
        }
        const auto updatePool = [&] {
            selection.setColumns(library.albums()->order().columns, library.snapshot());
        };
        QObject::connect(library.albums(), &yalahoo::AlbumListModel::columnsChanged, &selection,
                         updatePool);
        QObject::connect(&selection, &yalahoo::SelectionController::selectionChanged, &library,
                         [&] {
                             library.navigate(selection.albumId());
                         });
        QObject::connect(&library, &yalahoo::LibrarySession::navigationChanged, &selection, [&] {
            if (library.navigationAlbumId() != selection.albumId())
                selection.select(library.navigationAlbumId());
        });
        updatePool();
        scene.setSelection(&selection);
    }
    yalahoo::DetailModel detailModel;
    yalahoo::PresentationController presentation(motion.clock(),
                                                 yalahoo::detailPorts(scene, selection, library));
    QObject::connect(&presentation, &yalahoo::PresentationController::prepared, &detailModel, [&] {
        detailModel.prepare(library);
    });
    QObject::connect(&presentation, &yalahoo::PresentationController::changed, &inputRouter, [&] {
        inputRouter.setEnabled(presentation.archiveEnabled());
        if (!presentation.detailEnabled())
            selection.cancelDrag();
    });
    QObject::connect(&motion, &yalahoo::MotionController::reducedMotionChanged, &presentation, [&] {
        presentation.setReducedMotion(motion.reducedMotion());
    });
    QObject::connect(&selection, &yalahoo::SelectionController::libraryChanged, &presentation,
                     &yalahoo::PresentationController::reset);
    presentation.setReducedMotion(motion.reducedMotion());
    yalahoo::TrackFocusController trackFocus(&library, &presentation, motion.clock());
    yalahoo::PanelController panels(motion.clock());
    const auto updatePanelMotion = [&] {
        trackFocus.setReducedMotion(motion.reducedMotion());
        panels.setReducedMotion(motion.reducedMotion());
    };
    updatePanelMotion();
    QObject::connect(&motion, &yalahoo::MotionController::reducedMotionChanged, &panels,
                     updatePanelMotion);
    QObject::connect(&panels, &yalahoo::PanelController::changed, &trackFocus,
                     &yalahoo::TrackFocusController::cancel);
    QObject::connect(&trackFocus, &yalahoo::TrackFocusController::navigationRequested, &overview,
                     [&] {
                         overview.setActive(false);
                     });
    overview.bind(&scene, &selection, &presentation);
    QObject::connect(&inputRouter, &yalahoo::InputRouter::overviewToggleRequested, &overview,
                     &yalahoo::OverviewController::toggle);
    QObject::connect(&inputRouter, &yalahoo::InputRouter::overviewReturnRequested, &overview, [&] {
        overview.setActive(false);
    });
    QObject::connect(&inputRouter, &yalahoo::InputRouter::inputReceived, &overview,
                     &yalahoo::OverviewController::collapse);
    const auto updateOpeningInput = [&] {
        inputRouter.setEnabled(!opening.active() && presentation.archiveEnabled()
                               && !panels.surface()->shown());
    };
    QObject::connect(panels.surface(), &yalahoo::SurfaceTransition::changed, &inputRouter,
                     updateOpeningInput);
    QObject::connect(&opening, &yalahoo::OpeningController::changed, &inputRouter,
                     updateOpeningInput);
    QObject::connect(&presentation, &yalahoo::PresentationController::changed, &inputRouter,
                     updateOpeningInput);
    QObject::connect(&opening, &yalahoo::OpeningController::started, &presentation, [&] {
        panels.close();
        trackFocus.cancel();
        overview.setActive(false);
        presentation.reset();
        updateOpeningInput();
    });
    if (!verificationRun && !demonstration) {
        auto soundCell = std::make_shared<yalahoo::ArchiveCell>(selection.cell());
        QObject::connect(&selection, &yalahoo::SelectionController::selectionChanged, &sounds,
                         [&, soundCell] {
                             const auto cell = selection.cell();
                             sounds.trigger(cell.lane != soundCell->lane ? u"column"_s : u"tick"_s);
                             *soundCell = cell;
                         });
        QObject::connect(&panels, &yalahoo::PanelController::changed, &sounds, [&] {
            sounds.trigger(panels.panel().isEmpty() ? u"page-close"_s : u"page-open"_s);
        });
        QObject::connect(&presentation, &yalahoo::PresentationController::focusRequested, &sounds,
                         [&](bool detail) {
                             sounds.trigger(detail ? u"open"_s : u"back"_s);
                         });
        background.start();
    }
    QStringList deploymentWarnings;
    QQmlApplicationEngine engine;
    if (appVerification) {
        QObject::connect(&engine, &QQmlEngine::warnings, &engine,
                         [&](const QList<QQmlError> &warnings) {
                             for (const auto &warning : warnings)
                                 deploymentWarnings.append(warning.toString());
                         });
    }
    const bool usesApp = !verificationRun || appVerification;
    QVariantMap presentationProperties;
    if (usesApp) {
        if (!verificationRun || parser.isSet(u"verify-app"_s)) {
            presentationProperties.insert(u"openingController"_s, QVariant::fromValue(&opening));
            presentationProperties.insert(u"startupCoordinator"_s, QVariant::fromValue(&startup));
        }
        presentationProperties.insert(u"presentationController"_s,
                                      QVariant::fromValue(&presentation));
        presentationProperties.insert(u"playbackController"_s, QVariant::fromValue(&playback));
        presentationProperties.insert(u"backgroundMusicController"_s,
                                      QVariant::fromValue(&background));
        presentationProperties.insert(u"interfaceSoundController"_s, QVariant::fromValue(&sounds));
        presentationProperties.insert(u"trackFocusController"_s, QVariant::fromValue(&trackFocus));
        presentationProperties.insert(u"panelController"_s, QVariant::fromValue(&panels));
        presentationProperties.insert(u"libraryStore"_s, QVariant::fromValue(libraryStore.get()));
        presentationProperties.insert(u"detailModel"_s, QVariant::fromValue(&detailModel));
        presentationProperties.insert(u"overviewController"_s, QVariant::fromValue(&overview));
    }
    auto initialProperties = presentationProperties;
    const bool verificationFrameless = appVerification
#ifdef YALAHOO_REVIEW
            || parser.isSet(u"verify-integration"_s)
#endif
            ;
    const QVariantMap commonProperties(
            { { u"inputRouter"_s, QVariant::fromValue(&inputRouter) },
              { u"rulerController"_s, QVariant::fromValue(&rulerController) },
              { u"verificationRun"_s, verificationRun },
              { u"verificationFlags"_s,
                int(yalahoo::verification::windowFlags(verificationFrameless)) },
              { u"librarySession"_s, QVariant::fromValue(&library) },
              { u"motionController"_s, QVariant::fromValue(&motion) },
              { u"sceneAdapter"_s, QVariant::fromValue(&scene) },
              { u"preferencesError"_s, preferences ? preferences->error() : QString() },
              { u"geometryProbe"_s,
                parser.isSet(u"verify-runtime"_s) && !parser.isSet(u"verify-shading"_s) } });
    for (auto it = commonProperties.cbegin(); it != commonProperties.cend(); ++it)
        initialProperties.insert(it.key(), it.value());
#ifdef YALAHOO_REVIEW
    if (parser.isSet(u"verify-materials"_s)) {
        if (!parser.isSet(u"material-report"_s))
            return 12;
        return yalahoo::reviewMaterials(app, scene, parser.value(u"verify-materials"_s),
                                        parser.value(u"material-report"_s));
    }
#endif
    if (usesApp)
        initialProperties.insert(u"lightingLab"_s, parser.isSet(u"lighting-lab"_s));
    if (usesApp)
        initialProperties.insert(u"displayOutput"_s, QVariant::fromValue(&displayOutput));
    if (!verificationRun || appVerification)
        initialProperties.insert(u"settingsController"_s, QVariant::fromValue(&settings));
    engine.setInitialProperties(initialProperties);
    QObject::connect(
            &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
            [] {
                qCCritical(yalahoo::logApp) << "QML application creation failed";
                QCoreApplication::exit(2);
            },
            Qt::QueuedConnection);
    engine.loadFromModule("Yalahoo.Music",
                          verificationRun && !appVerification ? "BootstrapProbe" : "App");
    if (engine.rootObjects().isEmpty())
        return 2;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (!window)
        return 3;
    if (!verificationRun) {
        displayOutput.attach(*window);
        window->show();
    }
    if (verificationRun)
        yalahoo::verification::prepareWindow(*window, verificationFrameless);
#ifdef Q_OS_MACOS
    std::unique_ptr<yalahoo::ApplicationMenu> applicationMenu;
    if (!verificationRun) {
        applicationMenu = std::make_unique<yalahoo::ApplicationMenu>(
                *window,
                [&] {
                    panels.open(u"settings"_s);
                },
                [&] {
                    return !opening.active();
                });
    }
#endif
    inputRouter.attach(window);
    if (!verificationRun)
        opening.replay();
    if (preferences)
        QObject::connect(preferences.get(), &yalahoo::PreferencesStore::saveError, window,
                         [window](const QString &error) {
                             window->setProperty("preferencesError", error);
                         });
    if (preferences)
        QObject::connect(preferences.get(), &yalahoo::PreferencesStore::errorChanged, window,
                         [window, store = preferences.get()] {
                             window->setProperty("preferencesError", store->error());
                         });
    if (verificationRun
        && (window->isVisible() || !yalahoo::verification::windowPolicySatisfied(*window)))
        return 7;
    if (parser.isSet(u"verify-app"_s)) {
        QTimer::singleShot(0, &engine, [&] {
            const auto find = [](auto &&self, QQuickItem *item,
                                 const QString &name) -> QQuickItem * {
                if (item->objectName() == name)
                    return item;
                for (auto *child : item->childItems())
                    if (auto *found = self(self, child, name))
                        return found;
                return nullptr;
            };
            auto *surface = find(find, window->contentItem(), u"displaySurface"_s);
            auto *overlay = find(find, window->contentItem(), u"displayOverlay"_s);
            auto *sceneItem = find(find, window->contentItem(), u"nativeScene"_s);
            const bool passed = deploymentWarnings.isEmpty() && surface && overlay && sceneItem
                    && overlay->parentItem() == surface && surface->isAncestorOf(sceneItem)
                    && surface->size() == window->size() && !window->isVisible()
                    && yalahoo::verification::windowPolicySatisfied(*window)
                    && playback.source().isEmpty() && !playback.requested();
            if (!passed)
                qCCritical(yalahoo::logApp) << "Hidden product QML creation or composition failed"
                                            << deploymentWarnings;
            app.exit(passed ? 0 : 25);
        });
        return app.exec();
    }
    if (parser.isSet(u"verify-deployment"_s))
        return yalahoo::verifyDeployment(app, *window, parser.value(u"verify-deployment"_s),
                                         deploymentWarnings);
    if (parser.isSet(u"verify-overview"_s))
        return yalahoo::reviewOverview(app, *window, scene, overview, presentation, library, motion,
                                       parser.value(u"verify-overview"_s));
    if (parser.isSet(u"verify-detail"_s))
        return yalahoo::reviewDetail(app, *window, scene, library, presentation, motion,
                                     parser.value(u"verify-detail"_s));
    if (parser.isSet(u"verify-navigation"_s))
        return yalahoo::reviewNavigation(app, *window, scene, library, inputRouter, rulerController,
                                         parser.value(u"verify-navigation"_s));
    if (parser.isSet(u"verify-browse"_s))
        return yalahoo::reviewBrowse(app, *window, scene, library,
                                     parser.value(u"verify-browse"_s));
#ifdef YALAHOO_REVIEW
    if (parser.isSet(u"verify-integration"_s)) {
        bool ok = false;
        const auto seconds = parser.value(u"integration-seconds"_s).toDouble(&ok);
        if (!ok)
            return 20;
        return yalahoo::reviewIntegration(app, *window, scene,
                                          parser.value(u"verify-integration"_s), seconds);
    }
    if (parser.isSet(u"verify-scene"_s)) {
        if (!parser.isSet(u"scene-report"_s))
            return 9;
        return yalahoo::reviewScene(app, *window, scene, parser.value(u"verify-scene"_s),
                                    parser.value(u"scene-report"_s));
    }
#endif

    if (parser.isSet(u"verify-motion"_s)) {
        if (!parser.isSet(u"verify-runtime"_s))
            return 8;
        QObject::connect(motion.clock(), &yalahoo::FrameClock::frame, &app, [&](double dt, double) {
            if (awaitingResume) {
                resumeDiscarded = resumeDiscarded && dt == 0;
                ++resumeSamples;
                awaitingResume = false;
            }
        });
        QObject::connect(&motion, &yalahoo::MotionController::completed, &app,
                         [&](quint64 revision) {
                             completionRevisions.append(qint64(revision));
                         });
        motion.clock()->startOpening();
        motion.travel(7.44, 15.6, 12, 3);
        QTimer::singleShot(400, window, [&] {
            window->hide();
            pausedTime = motion.clock()->visualTime();
            pausedFrames = motion.clock()->frameCount();
        });
        QTimer::singleShot(700, window, [&] {
            motionEvidence.insert(u"hiddenPaused"_s,
                                  !motion.clock()->active()
                                          && pausedTime == motion.clock()->visualTime()
                                          && pausedFrames == motion.clock()->frameCount());
            awaitingResume = true;
            window->show();
        });
        QTimer::singleShot(1100, window, [&] {
            motion.clock()->setSpeed(2);
        });
        QTimer::singleShot(1200, window, [&] {
            window->showMinimized();
            pausedTime = motion.clock()->visualTime();
            pausedFrames = motion.clock()->frameCount();
        });
        QTimer::singleShot(1500, window, [&] {
            motionEvidence.insert(u"minimizedPaused"_s,
                                  !motion.clock()->active()
                                          && pausedTime == motion.clock()->visualTime()
                                          && pausedFrames == motion.clock()->frameCount());
            awaitingResume = true;
            window->showNormal();
        });
        QTimer::singleShot(1700, window, [&] {
            motion.travel(-.62, -5.2, -1, -1);
        });
        QTimer::singleShot(2000, window, [&] {
            motion.setReducedMotion(true);
        });
    }

    QObject::connect(
            window, &QQuickWindow::afterRendering, window,
            [&] {
                ++frames;
                api = static_cast<int>(window->rendererInterface()->graphicsApi());
            },
            Qt::DirectConnection);

    if (parser.isSet(u"verify-runtime"_s)) {
        window->show();
        const QString report = parser.value(u"verify-runtime"_s);
        QTimer::singleShot(1000, window, [window] {
            auto *button = window->findChild<QQuickItem *>(u"interactionProbe"_s);
            if (!button)
                return;
            const QPointF point =
                    button->mapToScene(QPointF(button->width() / 2, button->height() / 2));
            QMouseEvent press(QEvent::MouseButtonPress, point, window->mapToGlobal(point),
                              Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QMouseEvent release(QEvent::MouseButtonRelease, point, window->mapToGlobal(point),
                                Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(window, &press);
            QCoreApplication::sendEvent(window, &release);
        });
        auto reportWritten = std::make_shared<bool>(false);
        auto writeRuntimeReport = [&, report, reportWritten] {
            if (std::exchange(*reportWritten, true))
                return;
            const QImage image = window->grabWindow();
            const QRawFont raw = QRawFont::fromFont(QGuiApplication::font());
            const QString chinese = u"\u4e00\u5f20\u4e00\u5f20\uff0c\u6162\u6162\u542c"_s;
            bool chineseSupported = true;
            for (const QChar character : chinese)
                chineseSupported = chineseSupported && raw.supportsCharacter(character);
            const bool shaderAvailable = QFile::exists(u":/assets/shaders/probe.frag.qsb"_s);
            const bool qmlAvailable = QFile::exists(u":/qt/qml/Yalahoo/Music/App.qml"_s);
            const int interactions = window->property("interactionCount").toInt();
            const bool mediaAvailable = playback.isAvailable();
            const bool backend = api.load() == int(QQuickWindow::graphicsApi());
            QDir().mkpath(QFileInfo(report).absolutePath());
            const QString screenshot = QFileInfo(report).absolutePath()
                    + (parser.isSet(u"verify-motion"_s) ? u"/runtime-motion.png"_s
                                                        : u"/runtime.png"_s);
            const bool saved = !image.isNull() && image.save(screenshot);
            const auto *nativeScene = window->findChild<QQuickItem *>(u"nativeScene"_s);
            const bool nativeSceneAvailable = nativeScene != nullptr;
            const bool qmlMediaAvailable =
                    window->findChild<QObject *>(u"qmlMediaProbe"_s) != nullptr;
            const auto *libraryProbe = window->findChild<QObject *>(u"libraryDataProbe"_s);
            const QString libraryText =
                    libraryProbe ? libraryProbe->property("text").toString() : QString{ };
            int modelPixels = 0;
            QRect modelRegion;
            if (nativeScene && !image.isNull()) {
                const QRectF sceneRegion = nativeScene->mapRectToScene(
                        QRectF(0, 0, nativeScene->width(), nativeScene->height()));
                const qreal scaleX = qreal(image.width()) / window->width();
                const qreal scaleY = qreal(image.height()) / window->height();
                modelRegion = QRectF(sceneRegion.x() * scaleX, sceneRegion.y() * scaleY,
                                     sceneRegion.width() * scaleX, sceneRegion.height() * scaleY)
                                      .toAlignedRect()
                                      .intersected(image.rect());
            }
            for (int y = modelRegion.top(); !modelRegion.isEmpty() && y <= modelRegion.bottom();
                 ++y) {
                for (int x = modelRegion.left(); x <= modelRegion.right(); ++x) {
                    const QColor color = image.pixelColor(x, y);
                    if (parser.isSet(u"verify-shading"_s)
                                ? (color.red() > color.blue() + 20
                                   && color.red() > color.green() + 8)
                                : (color.green() > color.red() + 8 && color.green() < 165
                                   && color.red() > 40))
                        ++modelPixels;
                }
            }
            QStringList recorded;
            {
                const QMutexLocker lock(&diagnosticsMutex);
                recorded = diagnostics;
            }
            motionEvidence.insert(u"finalWindowVisibility"_s, int(window->visibility()));
            motionEvidence.insert(u"clockActive"_s, motion.clock()->active());
            motionEvidence.insert(u"applicationVisible"_s, motion.clock()->applicationVisible());
            const bool motionPassed = !parser.isSet(u"verify-motion"_s)
                    || (motionEvidence.value(u"hiddenPaused"_s).toBool()
                        && motionEvidence.value(u"minimizedPaused"_s).toBool() && resumeDiscarded
                        && resumeSamples == 2 && !motion.moving() && motion.rail() == -.62
                        && motion.column() == -5.2 && motion.clock()->openingTime() == 11.6
                        && !completionRevisions.isEmpty()
                        && completionRevisions.last().toInteger() == qint64(motion.revision())
                        && scene.reducedMotion() == motion.reducedMotion() && scene.settled()
                        && playback.source().isEmpty());
            const bool shaderDiagnostics =
                    std::any_of(recorded.cbegin(), recorded.cend(), [](const QString &s) {
                        return s.contains(u"Failed to compile"_s) || s.contains(u"QSpirvCompiler"_s)
                                || s.contains(u"TypeError"_s);
                    });
            const bool passed = yalahoo::verification::windowPolicySatisfied(*window)
                    && !shaderDiagnostics && motionPassed && frames > 0
                    && motion.clock()->frameCount() > 1 && motion.clock()->visualTime() > 0
                    && backend && interactions == 1 && mediaAvailable && shaderAvailable
                    && qmlAvailable && chineseSupported && saved && nativeSceneAvailable
                    && qmlMediaAvailable && modelPixels > 100 && libraryProbe
                    && (!parser.isSet(u"verify-filled"_s)
                        || (scene.selection() == &selection && scene.frame().instances.size() == 432
                            && library.albums()->rowCount() == 27
                            && library.playbackTrackId().isEmpty() && playback.source().isEmpty()))
                    && (!parser.isSet(u"verify-library"_s)
                        || (library.albums()->rowCount() == 21
                            && library.searchResults()->rowCount() == 28
                            && library.metadata().size() == 1 && library.playbackTrackId().isEmpty()
                            && playback.source().isEmpty()
                            && libraryText
                                    == u"\u533f\u540d\u5e93\u6a21\u578b \u00b7 21 \u5f20\u4e13\u8f91"_s));
            const QJsonObject evidence{
                { u"passed"_s, passed },
                { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
                { u"platformPlugin"_s, QGuiApplication::platformName() },
                { u"graphicsApi"_s, api.load() },
                { u"renderedFrames"_s, frames.load() },
                { u"windowTitle"_s, window->title() },
                { u"windowPolicy"_s, yalahoo::verification::windowPolicy(*window) },
                { u"devicePixelRatio"_s, window->devicePixelRatio() },
                { u"applicationName"_s, app.applicationName() },
                { u"displayName"_s, app.applicationDisplayName() },
                { u"dataDirectory"_s,
                  QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) },
                { u"controlRendering"_s, u"project-quick-templates"_s },
                { u"interactionCount"_s, interactions },
                { u"chineseGlyphs"_s, chineseSupported },
                { u"font"_s, raw.familyName() },
                { u"qmlResource"_s, qmlAvailable },
                { u"shaderResource"_s, shaderAvailable },
                { u"nativeSceneObject"_s, nativeSceneAvailable },
                { u"modelPixels"_s, modelPixels },
                { u"shadingProbe"_s, parser.isSet(u"verify-shading"_s) },
                { u"qmlMediaObject"_s, qmlMediaAvailable },
                { u"diagnostics"_s, QJsonArray::fromStringList(recorded) },
                { u"mediaAvailable"_s, mediaAvailable },
                { u"audioOutputCount"_s, QMediaDevices::audioOutputs().size() },
                { u"screenshotSaved"_s, saved },
                { u"sourceOpened"_s, false }
            };
            QJsonObject reportData = evidence;
            reportData.insert(u"visualFrameCount"_s, qint64(motion.clock()->frameCount()));
            reportData.insert(u"visualTime"_s, motion.clock()->visualTime());
            if (parser.isSet(u"verify-motion"_s)) {
                motionEvidence.insert(u"passed"_s, motionPassed);
                motionEvidence.insert(u"resumeDiscarded"_s, resumeDiscarded);
                motionEvidence.insert(u"resumeSamples"_s, resumeSamples);
                motionEvidence.insert(u"completionRevisions"_s, completionRevisions);
                motionEvidence.insert(u"finalRevision"_s, qint64(motion.revision()));
                motionEvidence.insert(u"openingTime"_s, motion.clock()->openingTime());
                motionEvidence.insert(u"mediaSourceEmpty"_s, playback.source().isEmpty());
                reportData.insert(u"motion"_s, motionEvidence);
            }
            reportData.insert(u"libraryAlbums"_s, library.albums()->rowCount());
            reportData.insert(u"searchResults"_s, library.searchResults()->rowCount());
            reportData.insert(u"metadataResults"_s, library.metadata().size());
            reportData.insert(u"playbackTrackId"_s, library.playbackTrackId());
            reportData.insert(u"libraryQmlText"_s, libraryText);
            reportData.insert(u"filledProbe"_s, parser.isSet(u"verify-filled"_s));
            if (scene.selection())
                reportData.insert(u"poolInstances"_s, scene.frame().instances.size());
            QSaveFile output(report);
            if (!output.open(QIODevice::WriteOnly)) {
                app.exit(4);
                return;
            }
            output.write(QJsonDocument(reportData).toJson());
            const bool committed = output.commit();
            app.exit(passed && committed ? 0 : 5);
        };
        if (parser.isSet(u"verify-motion"_s)) {
            QObject::connect(motion.clock(), &yalahoo::FrameClock::frame, window,
                             [&, writeRuntimeReport](double, double) {
                                 if (resumeSamples == 2 && motion.reducedMotion()
                                     && scene.settled())
                                     QTimer::singleShot(0, window, writeRuntimeReport);
                             });
            QTimer::singleShot(10000, window, writeRuntimeReport);
        } else {
            QTimer::singleShot(2500, window, writeRuntimeReport);
        }
    }
    const int result = app.exec();
    qCInfo(yalahoo::logApp) << "Application event loop stopped" << "exitCode=" << result;
    return result;
}
