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
#include "VerificationWindow.h"
#include "GraphicsBackend.h"
#include "DisplayOutput.h"
#include "DeploymentProbe.h"
#ifdef Q_OS_MACOS
#  include "ApplicationMenu.h"
#endif
#include "NavigationReview.h"
#include "DetailReview.h"
#include "OverviewController.h"
#include "OverviewReview.h"

#include "InputRouter.h"
#include "RulerController.h"
#include "Localization.h"
#ifdef YALAHOO_REVIEW
#  include "MaterialReview.h"
#endif

#include "ApplicationOptions.h"
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
// Nonzero process results are part of the standalone review command protocol.
enum ReviewExit {
    QmlCreationFailed = 2,
    RootWindowMissing = 3,
    AssetFontInitializationFailed = 6,
    InvalidCommandLine = std::to_underlying(yalahoo::ApplicationArgumentError::InvalidCommandLine),
    GraphicsBackendUnavailable = 22,
    AppVerificationFailed = 25
};

QQuickItem *findVisualItem(QQuickItem *item, const QString &name)
{
    if (item->objectName() == name)
        return item;
    for (auto *child : item->childItems()) {
        if (auto *found = findVisualItem(child, name))
            return found;
    }
    return nullptr;
}

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
int main(int argc, char **argv)
{
    yalahoo::initializeIdentity();
#ifdef YALAHOO_REVIEW
    constexpr bool ReviewEnabled = true;
#else
    constexpr bool ReviewEnabled = false;
#endif
    // Inspect product arguments without constructing a GUI platform. A Core application
    // is created only for commands that exit here, so normal startup owns one application.
    QStringList rawArguments;
    bool helpRequested = false;
    for (int i = 0; i < argc; ++i) {
        const auto argument = QString::fromLocal8Bit(argv[i]);
        rawArguments.append(argument);
        helpRequested = helpRequested || argument == "--help"_L1 || argument == "-h"_L1
                || argument == "--help-all"_L1;
    }
    if (helpRequested
        || !yalahoo::parseApplicationOptions(rawArguments, ReviewEnabled).error.isEmpty()) {
        QCoreApplication commandLineApp(argc, argv);
        const yalahoo::Localization localization;
        const auto parsed =
                yalahoo::parseApplicationOptions(commandLineApp.arguments(), ReviewEnabled);
        if (!parsed.error.isEmpty()) {
            const auto error = parsed.error.toUtf8();
            std::fwrite(error.constData(), 1, size_t(error.size()), stderr);
            std::fputc('\n', stderr);
            return std::to_underlying(parsed.errorCode);
        }
        const auto help = parsed.help.toUtf8();
        std::fwrite(help.constData(), 1, size_t(help.size()), stdout);
        return 0;
    }
    bool temporarySession = false;
    for (int i = 1; i < argc; ++i) {
        const QByteArray argument(argv[i]);
        if (argument.startsWith("--verify-"_ba)) {
            yalahoo::verification::prepareApplication();
            temporarySession = true;
        }
        temporarySession =
                temporarySession || argument == "--demo"_ba || argument == "--lighting-lab"_ba;
    }
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
    const auto parsed = yalahoo::parseApplicationOptions(app.arguments(), ReviewEnabled);
    if (!parsed.error.isEmpty()) {
        qCWarning(yalahoo::logApp) << parsed.error;
        return std::to_underlying(parsed.errorCode);
    }

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
    const auto &options = *parsed.options;
    const bool demonstration = options.demonstration();
    const bool appVerification = options.appVerification();
    const bool verificationRun = options.verificationRun();
    const bool persistentSession = options.persistent();
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
    if (!verificationRun || appVerification) {
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
        if (!verificationRun || options.verification == yalahoo::VerificationMode::App) {
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
    const bool verificationFrameless = appVerification;
    const QVariantMap commonProperties(
            { { u"inputRouter"_s, QVariant::fromValue(&inputRouter) },
              { u"rulerController"_s, QVariant::fromValue(&rulerController) },
              { u"verificationRun"_s, verificationRun },
              { u"verificationFlags"_s,
                int(yalahoo::verification::windowFlags(verificationFrameless)) },
              { u"librarySession"_s, QVariant::fromValue(&library) },
              { u"motionController"_s, QVariant::fromValue(&motion) },
              { u"sceneAdapter"_s, QVariant::fromValue(&scene) },
              { u"preferencesError"_s, preferences ? preferences->error() : QString() } });
    for (auto it = commonProperties.cbegin(); it != commonProperties.cend(); ++it)
        initialProperties.insert(it.key(), it.value());
#ifdef YALAHOO_REVIEW
    if (options.verification == yalahoo::VerificationMode::Materials) {
        return yalahoo::reviewMaterials(app, scene, options.verificationInput,
                                        options.materialReport);
    }
#endif
    if (usesApp)
        initialProperties.insert(u"lightingLab"_s,
                                 options.session == yalahoo::SessionMode::LightingLab);
    if (usesApp)
        initialProperties.insert(u"displayOutput"_s, QVariant::fromValue(&displayOutput));
    if (!verificationRun || appVerification)
        initialProperties.insert(u"settingsController"_s, QVariant::fromValue(&settings));
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
    if (!verificationRun) {
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
        return InvalidCommandLine;
    if (options.verification == yalahoo::VerificationMode::App) {
        QTimer::singleShot(0, &engine, [&] {
            auto *surface = findVisualItem(window->contentItem(), u"displaySurface"_s);
            auto *overlay = findVisualItem(window->contentItem(), u"displayOverlay"_s);
            auto *sceneItem = findVisualItem(window->contentItem(), u"nativeScene"_s);
            const bool passed = deploymentWarnings.isEmpty() && surface && overlay && sceneItem
                    && overlay->parentItem() == surface && surface->isAncestorOf(sceneItem)
                    && surface->size() == window->size() && !window->isVisible()
                    && yalahoo::verification::windowPolicySatisfied(*window)
                    && playback.source().isEmpty() && !playback.requested();
            if (!passed)
                qCCritical(yalahoo::logApp) << "Hidden product QML creation or composition failed"
                                            << deploymentWarnings;
            app.exit(passed ? 0 : AppVerificationFailed);
        });
        return app.exec();
    }
    if (options.verification == yalahoo::VerificationMode::Deployment)
        return yalahoo::verifyDeployment(app, *window, options.verificationInput,
                                         deploymentWarnings);
    if (options.verification == yalahoo::VerificationMode::Overview)
        return yalahoo::reviewOverview(app, *window, scene, overview, presentation, library, motion,
                                       options.verificationInput);
    if (options.verification == yalahoo::VerificationMode::Detail)
        return yalahoo::reviewDetail(app, *window, scene, library, presentation, motion,
                                     options.verificationInput);
    if (options.verification == yalahoo::VerificationMode::Navigation)
        return yalahoo::reviewNavigation(app, *window, scene, library, inputRouter, rulerController,
                                         options.verificationInput);
    const int result = app.exec();
    qCInfo(yalahoo::logApp) << "Application event loop stopped" << "exitCode=" << result;
    return result;
}
