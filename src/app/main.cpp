#include "AppIdentity.h"
#include "AssetCatalog.h"
#include "LibrarySession.h"
#include "OfflineMetadataProvider.h"

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
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"librarySession", QVariant::fromValue(&library)}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(2); }, Qt::QueuedConnection);
    engine.loadFromModule("Yalahoo.Music", "App");
    if (engine.rootObjects().isEmpty())
        return 2;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (!window)
        return 3;

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
            const QString screenshot = QFileInfo(report).absolutePath() + QStringLiteral("/runtime.png");
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
            const bool passed = frames > 0 && backend && interactions == 1 && mediaAvailable
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
