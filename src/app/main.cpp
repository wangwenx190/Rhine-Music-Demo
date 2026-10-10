#include "AppIdentity.h"
#include "AssetCatalog.h"
#include "LibrarySession.h"
#include "MotionController.h"
#include "OfflineMetadataProvider.h"
#include "SceneAdapter.h"
#ifdef YALAHOO_REVIEW
#include "SceneReview.h"
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
#include <cstdio>

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
#if defined(Q_OS_WIN)
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
#elif defined(Q_OS_MACOS)
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Metal);
#elif defined(Q_OS_LINUX)
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
#endif
    yalahoo::initializeIdentity();
    QGuiApplication app(argc, argv);
    if (!yalahoo::initializeAssetFonts()) {
        std::fprintf(stderr, "Embedded MiSans font registration failed\n");
        return 6;
    }
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("yalahOO!!! native Qt runtime probe"));
    parser.addHelpOption();
    parser.addOption({QStringLiteral("verify-runtime"), QStringLiteral("Write runtime evidence and exit"), QStringLiteral("report")});
    parser.addOption({QStringLiteral("verify-library"), QStringLiteral("Load anonymous mixed library for runtime verification only"), QStringLiteral("fixture")});
    parser.addOption({QStringLiteral("verify-motion"), QStringLiteral("Verify the production frame source, hide/restore and motion completion")});
#ifdef YALAHOO_REVIEW
    parser.addOption({QStringLiteral("verify-scene"), QStringLiteral("Run deterministic geometry fixtures"), QStringLiteral("fixture")});
    parser.addOption({QStringLiteral("scene-report"), QStringLiteral("Geometry review report"), QStringLiteral("report")});
#endif
    parser.process(app);
    if (parser.isSet(QStringLiteral("verify-runtime")))
        qInstallMessageHandler(recordDiagnostic);

    // Outlive the engine and its render-thread callbacks during teardown.
    std::atomic<int> frames{0};
    std::atomic<int> api{QSGRendererInterface::Unknown};
    QAudioOutput audio;
    QMediaPlayer media;
    media.setAudioOutput(&audio);
    yalahoo::OfflineMetadataProvider provider;
    yalahoo::LibrarySession library(&provider);
    if (parser.isSet(QStringLiteral("verify-library"))) {
        if (!parser.isSet(QStringLiteral("verify-runtime"))) return 7;
        QFile input(parser.value(QStringLiteral("verify-library")));
        if (!input.open(QIODevice::ReadOnly)) return 7;
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(input.readAll(), &parseError);
        yalahoo::LibrarySnapshot snapshot;
        QString error;
        if (parseError.error != QJsonParseError::NoError || !yalahoo::readLibrary(
            document.object().value("libraries").toObject().value("mixed").toObject(), snapshot, &error)) return 7;
        library.setSnapshot(std::move(snapshot));
        library.search(QStringLiteral("Typography"));
        if (!library.snapshot()->albums.isEmpty())
            library.requestMetadata(library.snapshot()->albums.first().id, QStringLiteral("introduction"));
    }
    yalahoo::MotionController motion;
    yalahoo::SceneAdapter scene(motion.clock());
    QObject::connect(&motion, &yalahoo::MotionController::reducedMotionChanged, &scene,
        [&] { scene.setReducedMotion(motion.reducedMotion()); });
    const auto updateApplicationVisibility = [&](Qt::ApplicationState state) {
        motion.clock()->setApplicationVisible(state != Qt::ApplicationHidden && state != Qt::ApplicationSuspended);
    };
    updateApplicationVisibility(app.applicationState());
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &motion, updateApplicationVisibility);
    QJsonObject motionEvidence;
    double pausedTime = 0;
    quint64 pausedFrames = 0;
    bool awaitingResume = false;
    bool resumeDiscarded = true;
    int resumeSamples = 0;
    QJsonArray completionRevisions;
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"librarySession", QVariant::fromValue(&library)},
        {"motionController", QVariant::fromValue(&motion)}, {"sceneAdapter", QVariant::fromValue(&scene)}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(2); }, Qt::QueuedConnection);
    engine.loadFromModule("Yalahoo.Music", "App");
    if (engine.rootObjects().isEmpty())
        return 2;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (!window)
        return 3;
#ifdef YALAHOO_REVIEW
    if (parser.isSet(QStringLiteral("verify-scene"))) {
        if (!parser.isSet(QStringLiteral("scene-report"))) return 9;
        return yalahoo::reviewScene(app, *window, scene, parser.value(QStringLiteral("verify-scene")),
            parser.value(QStringLiteral("scene-report")));
    }
#endif

    if (parser.isSet(QStringLiteral("verify-motion"))) {
        if (!parser.isSet(QStringLiteral("verify-runtime"))) return 8;
        QObject::connect(motion.clock(), &yalahoo::FrameClock::frame, &app, [&](double dt, double) {
            if (awaitingResume) {
                resumeDiscarded = resumeDiscarded && dt == 0;
                ++resumeSamples;
                awaitingResume = false;
            }
        });
        QObject::connect(&motion, &yalahoo::MotionController::completed, &app, [&](quint64 revision) {
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
            motionEvidence.insert("hiddenPaused", !motion.clock()->active()
                && pausedTime == motion.clock()->visualTime() && pausedFrames == motion.clock()->frameCount());
            awaitingResume = true;
            window->show();
        });
        QTimer::singleShot(1100, window, [&] { motion.clock()->setSpeed(2); });
        QTimer::singleShot(1200, window, [&] {
            window->showMinimized();
            pausedTime = motion.clock()->visualTime();
            pausedFrames = motion.clock()->frameCount();
        });
        QTimer::singleShot(1500, window, [&] {
            motionEvidence.insert("minimizedPaused", !motion.clock()->active()
                && pausedTime == motion.clock()->visualTime() && pausedFrames == motion.clock()->frameCount());
            awaitingResume = true;
            window->showNormal();
        });
        QTimer::singleShot(1700, window, [&] { motion.travel(-.62, -5.2, -1, -1); });
        QTimer::singleShot(2000, window, [&] { motion.setReducedMotion(true); });
    }

    QObject::connect(window, &QQuickWindow::afterRendering, window, [&] {
        ++frames;
        api = static_cast<int>(window->rendererInterface()->graphicsApi());
    }, Qt::DirectConnection);

    if (parser.isSet(QStringLiteral("verify-runtime"))) {
        const QString report = parser.value(QStringLiteral("verify-runtime"));
        QTimer::singleShot(1000, window, [window] {
            auto *button = window->findChild<QQuickItem *>(QStringLiteral("interactionProbe"));
            if (!button)
                return;
            const QPointF point = button->mapToScene(QPointF(button->width() / 2, button->height() / 2));
            QMouseEvent press(QEvent::MouseButtonPress, point, window->mapToGlobal(point),
                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QMouseEvent release(QEvent::MouseButtonRelease, point, window->mapToGlobal(point),
                Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(window, &press);
            QCoreApplication::sendEvent(window, &release);
        });
        QTimer::singleShot(2500, window, [&, report] {
            const QImage image = window->grabWindow();
            const QRawFont raw = QRawFont::fromFont(QGuiApplication::font());
            const QString chinese = QStringLiteral("一张一张，慢慢听");
            bool chineseSupported = true;
            for (const QChar character : chinese)
                chineseSupported = chineseSupported && raw.supportsCharacter(character);
            const bool shaderAvailable = QFile::exists(QStringLiteral(":/assets/shaders/probe.frag.qsb"));
            const bool qmlAvailable = QFile::exists(QStringLiteral(":/qt/qml/Yalahoo/Music/App.qml"));
            const int interactions = window->property("interactionCount").toInt();
            const bool mediaAvailable = media.isAvailable();
            bool backend = QSGRendererInterface::isApiRhiBased(static_cast<QSGRendererInterface::GraphicsApi>(api.load()));
#if defined(Q_OS_WIN)
            backend = api == QSGRendererInterface::Direct3D11;
#endif
            QDir().mkpath(QFileInfo(report).absolutePath());
            const QString screenshot = QFileInfo(report).absolutePath() + (parser.isSet(QStringLiteral("verify-motion"))
                ? QStringLiteral("/runtime-motion.png") : QStringLiteral("/runtime.png"));
            const bool saved = !image.isNull() && image.save(screenshot);
            const auto *view3D = window->findChild<QQuickItem *>(QStringLiteral("quick3DProbe"));
            const bool quick3DAvailable = view3D != nullptr;
            const bool qmlMediaAvailable = window->findChild<QObject *>(QStringLiteral("qmlMediaProbe")) != nullptr;
            const auto *libraryProbe = window->findChild<QObject *>(QStringLiteral("libraryDataProbe"));
            const QString libraryText = libraryProbe ? libraryProbe->property("text").toString() : QString{};
            int modelPixels = 0;
            QRect modelRegion;
            if (view3D && !image.isNull()) {
                const QRectF sceneRegion = view3D->mapRectToScene(QRectF(0, 0, view3D->width(), view3D->height()));
                const qreal scaleX = qreal(image.width()) / window->width();
                const qreal scaleY = qreal(image.height()) / window->height();
                modelRegion = QRectF(sceneRegion.x() * scaleX, sceneRegion.y() * scaleY,
                    sceneRegion.width() * scaleX, sceneRegion.height() * scaleY).toAlignedRect().intersected(image.rect());
            }
            for (int y = modelRegion.top(); !modelRegion.isEmpty() && y <= modelRegion.bottom(); ++y) {
                for (int x = modelRegion.left(); x <= modelRegion.right(); ++x) {
                    const QColor color = image.pixelColor(x, y);
                    if (color.green() > color.red() + 8 && color.green() < 165 && color.red() > 40)
                        ++modelPixels;
                }
            }
            QStringList recorded;
            {
                const QMutexLocker lock(&diagnosticsMutex);
                recorded = diagnostics;
            }
            const bool motionPassed = !parser.isSet(QStringLiteral("verify-motion"))
                || (motionEvidence.value("hiddenPaused").toBool() && motionEvidence.value("minimizedPaused").toBool()
                    && resumeDiscarded && resumeSamples == 2 && !motion.moving()
                    && motion.rail() == -.62 && motion.column() == -5.2 && motion.clock()->openingTime() == 11.6
                    && !completionRevisions.isEmpty() && completionRevisions.last().toInteger() == qint64(motion.revision())
                    && scene.reducedMotion() == motion.reducedMotion() && scene.settled()
                    && media.source().isEmpty());
            const bool passed = motionPassed && frames > 0 && motion.clock()->frameCount() > 1 && motion.clock()->visualTime() > 0
                && backend && interactions == 1 && mediaAvailable
                && shaderAvailable && qmlAvailable && chineseSupported && saved
                && quick3DAvailable && qmlMediaAvailable && modelPixels > 100 && libraryProbe
                && (!parser.isSet(QStringLiteral("verify-library")) || (library.albums()->rowCount() == 21
                    && library.searchResults()->rowCount() == 28 && library.metadata().size() == 1
                    && library.playbackTrackId().isEmpty() && media.source().isEmpty()
                    && libraryText == QStringLiteral("匿名库模型 · 21 张专辑")));
            const QJsonObject evidence{
                {"passed", passed}, {"qtVersion", QString::fromLatin1(qVersion())},
                {"platformPlugin", QGuiApplication::platformName()}, {"graphicsApi", api.load()},
                {"renderedFrames", frames.load()}, {"windowTitle", window->title()},
                {"devicePixelRatio",window->devicePixelRatio()},
                {"applicationName", app.applicationName()}, {"displayName", app.applicationDisplayName()},
                {"dataDirectory", QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)},
                {"controlsStyle", QQuickStyle::name()}, {"interactionCount", interactions},
                {"chineseGlyphs", chineseSupported}, {"font", raw.familyName()},
                {"qmlResource", qmlAvailable}, {"shaderResource", shaderAvailable},
                {"quick3DObject", quick3DAvailable}, {"modelPixels", modelPixels},
                {"qmlMediaObject", qmlMediaAvailable}, {"diagnostics", QJsonArray::fromStringList(recorded)},
                {"mediaAvailable", mediaAvailable}, {"audioOutputCount", QMediaDevices::audioOutputs().size()},
                {"screenshotSaved", saved}, {"sourceOpened", false}
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
            QSaveFile output(report);
            if (!output.open(QIODevice::WriteOnly)) {
                app.exit(4);
                return;
            }
            output.write(QJsonDocument(reportData).toJson());
            const bool committed = output.commit();
            app.exit(passed && committed ? 0 : 5);
        });
    }
    return app.exec();
}
