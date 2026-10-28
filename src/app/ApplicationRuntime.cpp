#include "AudioPreferencesBinding.h"
#include "FrameClock.h"
#include "SelectionController.h"
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
#include "LibraryPersistence.h"
#include "PresentationController.h"
#include "DetailModel.h"
#include "TrackFocusController.h"
#include "PanelController.h"
#include "DetailPorts.h"
#include "MotionController.h"
#include "OpeningController.h"
#include "StartupCoordinator.h"
#include "OfflineMetadataProvider.h"
#include "OnlineMetadataProvider.h"
#include <QNetworkProxyFactory>
#include "PreferencesStore.h"
#include "SingleInstance.h"
#include "SettingsController.h"
#include "DemoLibrary.h"
#include "SceneAdapter.h"
#include "GraphicsBackend.h"
#include "DisplayOutput.h"
#ifdef Q_OS_MACOS
#  include "ApplicationMenu.h"
#endif
#include "OverviewController.h"

#include "InputRouter.h"
#include "RulerController.h"
#include "Localization.h"

#include "ApplicationRuntime.h"
#include <QDir>
#include <QIcon>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
#include <memory>
#include <utility>

using namespace Qt::StringLiterals;

namespace {
// Startup failures retain the command-line exit protocol.
enum StartupExit {
    QmlCreationFailed = 2,
    RootWindowMissing = 3,
    AssetFontInitializationFailed = 6,
    InvalidCommandLine = std::to_underlying(yalahoo::ApplicationArgumentError::InvalidCommandLine),
    GraphicsBackendUnavailable = 22
};

bool initializeLibraryCommands(const yalahoo::ApplicationOptions &options,
                               yalahoo::LibraryStore &store, yalahoo::PreferencesStore &preferences)
{
    if (!options.preferencesImport.isEmpty()
        && !preferences.importJson(QUrl::fromLocalFile(options.preferencesImport)))
        return false;
    store.load();
    if (!options.legacyImport.isEmpty()) {
        if (!store.importLegacy(QUrl::fromLocalFile(options.legacyImport)))
            return false;
    } else if (!options.musicRoots.isEmpty()) {
        QList<QUrl> roots;
        for (const auto &path : options.musicRoots) {
            roots.append(QUrl::fromLocalFile(path));
        }
        if (!store.setRoots(roots))
            return false;
    } else if (options.rescan) {
        const auto once = std::make_shared<QMetaObject::Connection>();
        *once = QObject::connect(&store, &yalahoo::LibraryStore::snapshotReady, &store,
                                 [store = &store, once] {
                                     QObject::disconnect(*once);
                                     store->rescan();
                                 });
    }
    return true;
}
}
int yalahoo::runApplication(int argc, char **argv, const ApplicationOptions &options,
                            const ApplicationLaunch &launch)
{
    const bool temporarySession = !launch.persistent;
    std::unique_ptr<QTemporaryDir> temporaryLogs;
    yalahoo::LoggingSession::Options loggingOptions;
    if (temporarySession) {
        temporaryLogs = std::make_unique<QTemporaryDir>();
        if (!temporaryLogs->isValid()) {
            std::fputs("Cannot create isolated session logs\n", stderr);
            return InvalidCommandLine;
        }
        loggingOptions.directory = temporaryLogs->filePath(u"logs"_s);
    }
    const yalahoo::LoggingSession logging(std::move(loggingOptions));
    qCInfo(yalahoo::logApp) << "Application starting" << "Qt=" << qVersion()
                            << "platform=" << QSysInfo::prettyProductName()
                            << "architecture=" << QSysInfo::currentCpuArchitecture();
    if (!yalahoo::selectGraphicsBackend())
        return GraphicsBackendUnavailable;
    QGuiApplication app(argc, argv);
    auto globalFont = QGuiApplication::font();
    globalFont.setStyleStrategy(
            QFont::StyleStrategy(QFont::PreferAntialias | QFont::PreferQuality));
    globalFont.setHintingPreference(QFont::PreferNoHinting);
    QGuiApplication::setFont(globalFont);
    yalahoo::DisplayOutput displayOutput;
    const yalahoo::Localization localization;

    if (!yalahoo::probeOptionalGraphicsBackend())
        return GraphicsBackendUnavailable;
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
        return AssetFontInitializationFailed;
    }
    const bool persistentSession = launch.persistent;
    // Keep the ownership lock until library and final preference writes have finished.
    yalahoo::SingleInstance singleInstance;
    std::unique_ptr<yalahoo::PreferencesStore> preferences;
    QString dataRoot;
    if (persistentSession) {
        dataRoot = !options.dataRoot.isEmpty()
                ? options.dataRoot
                : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (!QDir::isAbsolutePath(dataRoot)) {
            qCWarning(yalahoo::logApp, "--data-dir requires an absolute path");
            return InvalidCommandLine;
        }
        preferences = std::make_unique<yalahoo::PreferencesStore>(dataRoot);
        const auto result = singleInstance.start(
                dataRoot, preferences->values().value(u"singleInstance"_s).toBool());
        if (result == yalahoo::SingleInstance::Result::Secondary)
            return 0;
        if (result == yalahoo::SingleInstance::Result::Error) {
            qCCritical(yalahoo::logApp) << singleInstance.error();
            return InvalidCommandLine;
        }
    }
    QNetworkProxyFactory::setUseSystemConfiguration(true);
    std::unique_ptr<yalahoo::IMetadataProvider> provider;
    if (!persistentSession)
        provider = std::make_unique<yalahoo::OfflineMetadataProvider>();
    else
        provider = std::make_unique<yalahoo::OnlineMetadataProvider>();
    yalahoo::LibrarySession library(provider.get());
    yalahoo::BackgroundMusicController background;
    yalahoo::InterfaceSoundController sounds;
    yalahoo::PlaybackController playback(&library);
    playback.setBackgroundMusic(&background);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &playback, [&] {
        playback.stop();
        background.stop();
        sounds.stop();
    });
    yalahoo::MotionController motion;
    yalahoo::OverviewController overview(&motion);
    yalahoo::SelectionController selection(&motion);
    yalahoo::SceneAdapter scene(motion.clock());
    yalahoo::OpeningController opening(&motion, &scene, &selection);
    yalahoo::StartupCoordinator startup(&library, &opening, motion.clock(), persistentSession);
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
    std::unique_ptr<yalahoo::AudioPreferencesBinding> audioPreferences;
    std::unique_ptr<yalahoo::LibraryStore> libraryStore;
    std::unique_ptr<yalahoo::LibraryPersistence> libraryPersistence;
    if (persistentSession) {
        audioPreferences = std::make_unique<yalahoo::AudioPreferencesBinding>(
                *preferences, playback, background, sounds);
        libraryStore = std::make_unique<yalahoo::LibraryStore>(dataRoot);
        libraryPersistence = std::make_unique<yalahoo::LibraryPersistence>(
                *libraryStore, *static_cast<yalahoo::OnlineMetadataProvider *>(provider.get()),
                library, startup);
        if (!initializeLibraryCommands(options, *libraryStore, *preferences))
            return InvalidCommandLine;
    }
    yalahoo::SettingsController settings(preferences.get(), &library, &motion, &scene, &selection);
    if (launch.bindLibrary) {
        // Anonymous sessions never read the persistent library.
        if (launch.anonymousLibrary) {
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
    QObject::connect(&library, &yalahoo::LibrarySession::albumMetadataChanged, &detailModel,
                     [&](const QString &albumId) {
                         if (detailModel.album().value(u"id"_s).toString() == albumId)
                             detailModel.prepare(library, true);
                     });
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
    if (persistentSession) {
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
    QQmlApplicationEngine engine;
    if (launch.observeEngine)
        launch.observeEngine(engine);
    ApplicationContext context{ app,     engine, scene,    overview,    presentation,
                                library, motion, playback, inputRouter, rulerController };
    if (launch.beforeLoad)
        if (const auto result = launch.beforeLoad(context))
            return *result;
    QVariantMap presentationProperties;
    {
        if (launch.bindOpening) {
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
    const QVariantMap commonProperties(
            { { u"inputRouter"_s, QVariant::fromValue(&inputRouter) },
              { u"rulerController"_s, QVariant::fromValue(&rulerController) },
              { u"librarySession"_s, QVariant::fromValue(&library) },
              { u"motionController"_s, QVariant::fromValue(&motion) },
              { u"sceneAdapter"_s, QVariant::fromValue(&scene) },
              { u"preferencesError"_s, preferences ? preferences->error() : QString() } });
    for (auto it = commonProperties.cbegin(); it != commonProperties.cend(); ++it)
        initialProperties.insert(it.key(), it.value());
    initialProperties.insert(u"lightingLab"_s, options.session == SessionMode::LightingLab);
    initialProperties.insert(u"displayOutput"_s, QVariant::fromValue(&displayOutput));
    initialProperties.insert(u"settingsController"_s, QVariant::fromValue(&settings));
    for (auto it = launch.initialProperties.cbegin(); it != launch.initialProperties.cend(); ++it)
        initialProperties.insert(it.key(), it.value());
    engine.setInitialProperties(initialProperties);
    QObject::connect(
            &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
            [] {
                qCCritical(yalahoo::logApp) << "QML application creation failed";
                QCoreApplication::exit(QmlCreationFailed);
            },
            Qt::QueuedConnection);
    engine.loadFromModule("Yalahoo.Music", "App");
    if (engine.rootObjects().isEmpty())
        return QmlCreationFailed;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (!window)
        return RootWindowMissing;
    if (launch.automaticPresentation) {
        displayOutput.attach(*window);
        window->show();
        QObject::connect(&singleInstance, &yalahoo::SingleInstance::activationRequested, window,
                         [window] {
                             if (window->windowState() == Qt::WindowMinimized)
                                 window->showNormal();
                             window->show();
                             window->raise();
                             window->requestActivate();
                         });
    }
#ifdef Q_OS_MACOS
    std::unique_ptr<yalahoo::ApplicationMenu> applicationMenu;
    if (launch.automaticPresentation) {
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
    if (launch.automaticPresentation)
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
    context.window = window;
    if (launch.afterLoad)
        if (const auto result = launch.afterLoad(context))
            return *result;
    const int result = app.exec();
    qCInfo(yalahoo::logApp) << "Application event loop stopped" << "exitCode=" << result;
    return result;
}
