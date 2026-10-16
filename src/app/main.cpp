#include "AppIdentity.h"
#include "AssetCatalog.h"
#include "LibrarySession.h"
#include "PresentationController.h"
#include "DetailModel.h"
#include "DetailPorts.h"
#include "MotionController.h"
#include "OpeningController.h"
#include "OfflineMetadataProvider.h"
#include "PreferencesStore.h"
#include "SceneAdapter.h"
#include "VerificationWindow.h"
#include "GraphicsBackend.h"
#include "BrowseReview.h"
#include "NavigationReview.h"
#include "DetailReview.h"
#include "OverviewController.h"
#include "OverviewReview.h"

#include "InputRouter.h"
#include "RulerController.h"
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
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRawFont>
#include <QSaveFile>
#include <QSGRendererInterface>
#include <QStandardPaths>
#include <QTimer>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <utility>

namespace {
QMutex diagnosticsMutex;
QStringList diagnostics;
void recordDiagnostic(QtMsgType, const QMessageLogContext &, const QString &message)
{
    const QMutexLocker lock(&diagnosticsMutex);
    diagnostics.append(message);
    std::fprintf(stderr, "%s\n", message.toUtf8().constData());
}
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        if (QByteArray(argv[i]).startsWith("--verify-")) {
            yalahoo::verification::prepareApplication();
        }
    }
    if (!yalahoo::selectGraphicsBackend())
        return 22;
    yalahoo::initializeIdentity();
    QGuiApplication app(argc, argv);
    if (!yalahoo::initializeAssetFonts()) {
        std::fprintf(stderr, "Embedded MiSans font registration failed\n");
        return 6;
    }
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("yalahOO!!! native music archive"));
    parser.addHelpOption();
    parser.addOption({ QStringLiteral("verify-overview"),
                       QStringLiteral("Verify production overview and exit"),
                       QStringLiteral("directory") });
    parser.addOption({ QStringLiteral("verify-detail"),
                       QStringLiteral("Verify production detail motion and exit"),
                       QStringLiteral("directory") });
    parser.addOption({ QStringLiteral("verify-navigation"),
                       QStringLiteral("Verify production navigation and exit"),
                       QStringLiteral("directory") });
    parser.addOption({ QStringLiteral("verify-browse"),
                       QStringLiteral("Verify production browse UI and exit"),
                       QStringLiteral("directory") });
    parser.addOption({ QStringLiteral("data-dir"), QStringLiteral("Override the native data root"),
                       QStringLiteral("directory") });
    parser.addOption({ QStringLiteral("verify-runtime"),
                       QStringLiteral("Write runtime evidence and exit"),
                       QStringLiteral("report") });
    parser.addOption({ QStringLiteral("verify-library"),
                       QStringLiteral("Load anonymous mixed library for runtime verification only"),
                       QStringLiteral("fixture") });
    parser.addOption(
            { QStringLiteral("verify-filled"),
              QStringLiteral("Exercise the production filled demonstration with verify-runtime") });
    parser.addOption(
            { QStringLiteral("verify-motion"),
              QStringLiteral(
                      "Verify the production frame source, hide/restore and motion completion") });
    parser.addOption(
            { QStringLiteral("verify-shading"),
              QStringLiteral(
                      "Exercise production material/shadow/AO rendering with verify-runtime") });
#ifdef YALAHOO_REVIEW
    parser.addOption({ QStringLiteral("verify-integration"),
                       QStringLiteral("Measure RHI production integration and exit"),
                       QStringLiteral("directory") });
    parser.addOption({ QStringLiteral("integration-seconds"),
                       QStringLiteral("Production sampling duration (12-120 seconds)"),
                       QStringLiteral("seconds"), QStringLiteral("12") });
    parser.addOption({ QStringLiteral("verify-materials"),
                       QStringLiteral("Verify source material state and native output"),
                       QStringLiteral("fixture") });
    parser.addOption({ QStringLiteral("material-report"),
                       QStringLiteral("Material source/state report"), QStringLiteral("report") });
    parser.addOption({ QStringLiteral("verify-scene"),
                       QStringLiteral("Run deterministic geometry fixtures"),
                       QStringLiteral("fixture") });
    parser.addOption({ QStringLiteral("scene-report"), QStringLiteral("Geometry review report"),
                       QStringLiteral("report") });
#endif
    parser.process(app);
    const bool appVerification = parser.isSet(QStringLiteral("verify-browse"))
            || parser.isSet(QStringLiteral("verify-navigation"))
            || parser.isSet(QStringLiteral("verify-detail"))
            || parser.isSet(QStringLiteral("verify-overview"));
    if (parser.isSet(QStringLiteral("verify-filled"))
        && (!parser.isSet(QStringLiteral("verify-runtime"))
            || parser.isSet(QStringLiteral("verify-library"))
            || parser.isSet(QStringLiteral("verify-motion"))))
        return 7;
    if (parser.isSet(QStringLiteral("verify-shading"))
        && !parser.isSet(QStringLiteral("verify-runtime")))
        return 7;
    if (parser.isSet(QStringLiteral("verify-runtime")) || appVerification
#ifdef YALAHOO_REVIEW
        || parser.isSet(QStringLiteral("verify-materials"))
#endif
    )
        qInstallMessageHandler(recordDiagnostic);

    // Outlive the engine and its render-thread callbacks during teardown.
    std::atomic<int> frames{ 0 };
    std::atomic<int> api{ QSGRendererInterface::Unknown };
    QAudioOutput audio;
    QMediaPlayer media;
    media.setAudioOutput(&audio);
    yalahoo::OfflineMetadataProvider provider;
    yalahoo::LibrarySession library(&provider);
    if (parser.isSet(QStringLiteral("verify-library"))) {
        if (!parser.isSet(QStringLiteral("verify-runtime")))
            return 7;
        QFile input(parser.value(QStringLiteral("verify-library")));
        if (!input.open(QIODevice::ReadOnly))
            return 7;
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(input.readAll(), &parseError);
        yalahoo::LibrarySnapshot snapshot;
        QString error;
        if (parseError.error != QJsonParseError::NoError
            || !yalahoo::readLibrary(
                    document.object().value("libraries").toObject().value("mixed").toObject(),
                    snapshot, &error))
            return 7;
        library.setSnapshot(std::move(snapshot));
        library.search(QStringLiteral("Typography"));
        if (!library.snapshot()->albums.isEmpty())
            library.requestMetadata(library.snapshot()->albums.first().id,
                                    QStringLiteral("introduction"));
    }
    yalahoo::MotionController motion;
    yalahoo::OverviewController overview(&motion);
    yalahoo::SelectionController selection(&motion);
    yalahoo::SceneAdapter scene(motion.clock());
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
    const bool verificationRun = parser.isSet(QStringLiteral("verify-runtime")) || appVerification
#ifdef YALAHOO_REVIEW
            || parser.isSet(QStringLiteral("verify-materials"))
            || parser.isSet(QStringLiteral("verify-scene"))
            || parser.isSet(QStringLiteral("verify-integration"))
#endif
            ;
    std::unique_ptr<yalahoo::PreferencesStore> preferences;
    if (!verificationRun) {
        const QString dataRoot = parser.isSet(QStringLiteral("data-dir"))
                ? parser.value(QStringLiteral("data-dir"))
                : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        preferences = std::make_unique<yalahoo::PreferencesStore>(dataRoot);
        selection.setRememberColumnPosition(preferences->rememberColumnPosition());
        selection.setRealistic(preferences->realistic());
        const auto saveNavigation = [&] {
            preferences->setNavigation(selection.rememberColumnPosition(), selection.realistic());
        };
        QObject::connect(&selection, &yalahoo::SelectionController::memoryChanged,
                         preferences.get(), saveNavigation);
        QObject::connect(&selection, &yalahoo::SelectionController::arrayModeChanged,
                         preferences.get(), saveNavigation);
    }
    if (!verificationRun || parser.isSet(QStringLiteral("verify-filled")) || appVerification) {
        // Local resource-only demonstration until the scanner is delivered in R21.
        yalahoo::LibrarySnapshot demo;
        const QStringList covers{ "square", "portrait", "landscape" };
        for (int lane = 0; lane < 9; ++lane) {
            const auto genreId = QStringLiteral("demo-genre-%1").arg(lane);
            yalahoo::Genre genre;
            genre.id = genreId;
            genre.name = QStringLiteral("演示收藏 %1").arg(lane + 1);
            demo.genres.append(genre);
            for (int row = 0; row < 3; ++row) {
                yalahoo::Album album;
                album.id = QStringLiteral("demo-album-%1-%2").arg(lane).arg(row);
                album.title = QStringLiteral("演示专辑 %1").arg(row + 1);
                album.genreId = genreId;
                album.coverUrl =
                        QUrl(QStringLiteral("qrc:/assets/demo-covers/%1.png").arg(covers[row]));
                demo.albums.append(album);
            }
        }
        library.setSnapshot(std::move(demo));
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
    overview.bind(&scene, &selection, &presentation);
    QObject::connect(&inputRouter, &yalahoo::InputRouter::overviewToggleRequested, &overview,
                     &yalahoo::OverviewController::toggle);
    QObject::connect(&inputRouter, &yalahoo::InputRouter::overviewReturnRequested, &overview, [&] {
        overview.setActive(false);
    });
    QObject::connect(&inputRouter, &yalahoo::InputRouter::inputReceived, &overview,
                     &yalahoo::OverviewController::collapse);
    yalahoo::OpeningController opening(&motion, &scene, &selection);
    const auto updateOpeningInput = [&] {
        inputRouter.setEnabled(!opening.active() && presentation.archiveEnabled());
    };
    QObject::connect(&opening, &yalahoo::OpeningController::changed, &inputRouter,
                     updateOpeningInput);
    QObject::connect(&presentation, &yalahoo::PresentationController::changed, &inputRouter,
                     updateOpeningInput);
    QObject::connect(&opening, &yalahoo::OpeningController::started, &presentation, [&] {
        overview.setActive(false);
        presentation.reset();
        updateOpeningInput();
    });
    QQmlApplicationEngine engine;
    const bool usesApp = !verificationRun || appVerification;
    QVariantMap presentationProperties;
    if (usesApp) {
        if (!verificationRun)
            presentationProperties.insert("openingController", QVariant::fromValue(&opening));
        presentationProperties.insert("presentationController", QVariant::fromValue(&presentation));
        presentationProperties.insert("detailModel", QVariant::fromValue(&detailModel));
        presentationProperties.insert("overviewController", QVariant::fromValue(&overview));
    }
    auto initialProperties = presentationProperties;
    const bool verificationFrameless = appVerification
#ifdef YALAHOO_REVIEW
            || parser.isSet(QStringLiteral("verify-integration"))
#endif
            ;
    const QVariantMap commonProperties(
            { { "inputRouter", QVariant::fromValue(&inputRouter) },
              { "rulerController", QVariant::fromValue(&rulerController) },
              { "verificationRun", verificationRun },
              { "verificationFlags",
                int(yalahoo::verification::windowFlags(verificationFrameless)) },
              { "librarySession", QVariant::fromValue(&library) },
              { "motionController", QVariant::fromValue(&motion) },
              { "sceneAdapter", QVariant::fromValue(&scene) },
              { "preferencesError", preferences ? preferences->error() : QString() },
              { "geometryProbe",
                parser.isSet(QStringLiteral("verify-runtime"))
                        && !parser.isSet(QStringLiteral("verify-shading")) } });
    for (auto it = commonProperties.cbegin(); it != commonProperties.cend(); ++it)
        initialProperties.insert(it.key(), it.value());
#ifdef YALAHOO_REVIEW
    if (parser.isSet(QStringLiteral("verify-materials"))) {
        if (!parser.isSet(QStringLiteral("material-report")))
            return 12;
        return yalahoo::reviewMaterials(app, scene,
                                        parser.value(QStringLiteral("verify-materials")),
                                        parser.value(QStringLiteral("material-report")));
    }
#endif
    engine.setInitialProperties(initialProperties);
    QObject::connect(
            &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
            [] {
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
    if (verificationRun)
        yalahoo::verification::prepareWindow(*window, verificationFrameless);
    inputRouter.attach(window);
    if (!verificationRun)
        opening.replay();
    if (preferences)
        QObject::connect(preferences.get(), &yalahoo::PreferencesStore::saveError, window,
                         [window](const QString &error) {
                             window->setProperty("preferencesError", error);
                         });
    if (verificationRun
        && (window->isVisible() || !yalahoo::verification::windowPolicySatisfied(*window)))
        return 7;
    if (parser.isSet(QStringLiteral("verify-overview")))
        return yalahoo::reviewOverview(app, *window, scene, overview, presentation, library, motion,
                                       parser.value(QStringLiteral("verify-overview")));
    if (parser.isSet(QStringLiteral("verify-detail")))
        return yalahoo::reviewDetail(app, *window, scene, library, presentation, motion,
                                     parser.value(QStringLiteral("verify-detail")));
    if (parser.isSet(QStringLiteral("verify-navigation")))
        return yalahoo::reviewNavigation(app, *window, scene, library, inputRouter, rulerController,
                                         parser.value(QStringLiteral("verify-navigation")));
    if (parser.isSet(QStringLiteral("verify-browse")))
        return yalahoo::reviewBrowse(app, *window, scene, library,
                                     parser.value(QStringLiteral("verify-browse")));
#ifdef YALAHOO_REVIEW
    if (parser.isSet(QStringLiteral("verify-integration"))) {
        bool ok = false;
        const auto seconds = parser.value(QStringLiteral("integration-seconds")).toDouble(&ok);
        if (!ok)
            return 20;
        return yalahoo::reviewIntegration(
                app, *window, scene, parser.value(QStringLiteral("verify-integration")), seconds);
    }
    if (parser.isSet(QStringLiteral("verify-scene"))) {
        if (!parser.isSet(QStringLiteral("scene-report")))
            return 9;
        return yalahoo::reviewScene(app, *window, scene,
                                    parser.value(QStringLiteral("verify-scene")),
                                    parser.value(QStringLiteral("scene-report")));
    }
#endif

    if (parser.isSet(QStringLiteral("verify-motion"))) {
        if (!parser.isSet(QStringLiteral("verify-runtime")))
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
            motionEvidence.insert("hiddenPaused",
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
            motionEvidence.insert("minimizedPaused",
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

    if (parser.isSet(QStringLiteral("verify-runtime"))) {
        window->show();
        const QString report = parser.value(QStringLiteral("verify-runtime"));
        QTimer::singleShot(1000, window, [window] {
            auto *button = window->findChild<QQuickItem *>(QStringLiteral("interactionProbe"));
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
            const QString chinese = QStringLiteral("一张一张，慢慢听");
            bool chineseSupported = true;
            for (const QChar character : chinese)
                chineseSupported = chineseSupported && raw.supportsCharacter(character);
            const bool shaderAvailable =
                    QFile::exists(QStringLiteral(":/assets/shaders/probe.frag.qsb"));
            const bool qmlAvailable =
                    QFile::exists(QStringLiteral(":/qt/qml/Yalahoo/Music/App.qml"));
            const int interactions = window->property("interactionCount").toInt();
            const bool mediaAvailable = media.isAvailable();
            bool backend = QSGRendererInterface::isApiRhiBased(
                    static_cast<QSGRendererInterface::GraphicsApi>(api.load()));
#if defined(Q_OS_WIN)
            backend = api
                    == (qgetenv("QSG_RHI_BACKEND") == "opengl" ? QSGRendererInterface::OpenGL
                                                               : QSGRendererInterface::Direct3D11);
#endif
            QDir().mkpath(QFileInfo(report).absolutePath());
            const QString screenshot = QFileInfo(report).absolutePath()
                    + (parser.isSet(QStringLiteral("verify-motion"))
                               ? QStringLiteral("/runtime-motion.png")
                               : QStringLiteral("/runtime.png"));
            const bool saved = !image.isNull() && image.save(screenshot);
            const auto *nativeScene =
                    window->findChild<QQuickItem *>(QStringLiteral("nativeScene"));
            const bool nativeSceneAvailable = nativeScene != nullptr;
            const bool qmlMediaAvailable =
                    window->findChild<QObject *>(QStringLiteral("qmlMediaProbe")) != nullptr;
            const auto *libraryProbe =
                    window->findChild<QObject *>(QStringLiteral("libraryDataProbe"));
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
                    if (parser.isSet(QStringLiteral("verify-shading"))
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
            motionEvidence.insert("finalWindowVisibility", int(window->visibility()));
            motionEvidence.insert("clockActive", motion.clock()->active());
            motionEvidence.insert("applicationVisible", motion.clock()->applicationVisible());
            const bool motionPassed = !parser.isSet(QStringLiteral("verify-motion"))
                    || (motionEvidence.value("hiddenPaused").toBool()
                        && motionEvidence.value("minimizedPaused").toBool() && resumeDiscarded
                        && resumeSamples == 2 && !motion.moving() && motion.rail() == -.62
                        && motion.column() == -5.2 && motion.clock()->openingTime() == 11.6
                        && !completionRevisions.isEmpty()
                        && completionRevisions.last().toInteger() == qint64(motion.revision())
                        && scene.reducedMotion() == motion.reducedMotion() && scene.settled()
                        && media.source().isEmpty());
            const bool shaderDiagnostics =
                    std::any_of(recorded.cbegin(), recorded.cend(), [](const QString &s) {
                        return s.contains("Failed to compile") || s.contains("QSpirvCompiler")
                                || s.contains("TypeError");
                    });
            const bool passed = yalahoo::verification::windowPolicySatisfied(*window)
                    && !shaderDiagnostics && motionPassed && frames > 0
                    && motion.clock()->frameCount() > 1 && motion.clock()->visualTime() > 0
                    && backend && interactions == 1 && mediaAvailable && shaderAvailable
                    && qmlAvailable && chineseSupported && saved && nativeSceneAvailable
                    && qmlMediaAvailable && modelPixels > 100 && libraryProbe
                    && (!parser.isSet(QStringLiteral("verify-filled"))
                        || (scene.selection() == &selection && scene.frame().instances.size() == 432
                            && library.albums()->rowCount() == 27
                            && library.playbackTrackId().isEmpty() && media.source().isEmpty()))
                    && (!parser.isSet(QStringLiteral("verify-library"))
                        || (library.albums()->rowCount() == 21
                            && library.searchResults()->rowCount() == 28
                            && library.metadata().size() == 1 && library.playbackTrackId().isEmpty()
                            && media.source().isEmpty()
                            && libraryText == QStringLiteral("匿名库模型 · 21 张专辑")));
            const QJsonObject evidence{
                { "passed", passed },
                { "qtVersion", QString::fromLatin1(qVersion()) },
                { "platformPlugin", QGuiApplication::platformName() },
                { "graphicsApi", api.load() },
                { "renderedFrames", frames.load() },
                { "windowTitle", window->title() },
                { "windowPolicy", yalahoo::verification::windowPolicy(*window) },
                { "devicePixelRatio", window->devicePixelRatio() },
                { "applicationName", app.applicationName() },
                { "displayName", app.applicationDisplayName() },
                { "dataDirectory",
                  QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) },
                { "controlsStyle", QQuickStyle::name() },
                { "interactionCount", interactions },
                { "chineseGlyphs", chineseSupported },
                { "font", raw.familyName() },
                { "qmlResource", qmlAvailable },
                { "shaderResource", shaderAvailable },
                { "nativeSceneObject", nativeSceneAvailable },
                { "modelPixels", modelPixels },
                { "shadingProbe", parser.isSet(QStringLiteral("verify-shading")) },
                { "qmlMediaObject", qmlMediaAvailable },
                { "diagnostics", QJsonArray::fromStringList(recorded) },
                { "mediaAvailable", mediaAvailable },
                { "audioOutputCount", QMediaDevices::audioOutputs().size() },
                { "screenshotSaved", saved },
                { "sourceOpened", false }
            };
            QJsonObject reportData = evidence;
            reportData.insert("visualFrameCount", qint64(motion.clock()->frameCount()));
            reportData.insert("visualTime", motion.clock()->visualTime());
            if (parser.isSet(QStringLiteral("verify-motion"))) {
                motionEvidence.insert("passed", motionPassed);
                motionEvidence.insert("resumeDiscarded", resumeDiscarded);
                motionEvidence.insert("resumeSamples", resumeSamples);
                motionEvidence.insert("completionRevisions", completionRevisions);
                motionEvidence.insert("finalRevision", qint64(motion.revision()));
                motionEvidence.insert("openingTime", motion.clock()->openingTime());
                motionEvidence.insert("mediaSourceEmpty", media.source().isEmpty());
                reportData.insert("motion", motionEvidence);
            }
            reportData.insert("libraryAlbums", library.albums()->rowCount());
            reportData.insert("searchResults", library.searchResults()->rowCount());
            reportData.insert("metadataResults", library.metadata().size());
            reportData.insert("playbackTrackId", library.playbackTrackId());
            reportData.insert("libraryQmlText", libraryText);
            reportData.insert("filledProbe", parser.isSet(QStringLiteral("verify-filled")));
            if (scene.selection())
                reportData.insert("poolInstances", scene.frame().instances.size());
            QSaveFile output(report);
            if (!output.open(QIODevice::WriteOnly)) {
                app.exit(4);
                return;
            }
            output.write(QJsonDocument(reportData).toJson());
            const bool committed = output.commit();
            app.exit(passed && committed ? 0 : 5);
        };
        if (parser.isSet(QStringLiteral("verify-motion"))) {
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
    return app.exec();
}
