#include "DeploymentProbe.h"
#include "VerificationWindow.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibraryInfo>
#include <QMediaPlayer>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSaveFile>
#include <QTimer>
#ifdef Q_OS_MACOS
#  include <mach-o/dyld.h>
#elif defined(Q_OS_LINUX)
#  include <link.h>
#endif

int yalahoo::verifyDeployment(QGuiApplication &app, QQuickWindow &window, const QString &reportPath,
                              const QStringList &qmlWarnings)
{
    QMediaPlayer media;
    bool finished = false;
    bool loaded = false;
    const auto finish = [&] {
        if (finished)
            return;
        finished = true;
        QJsonArray images;
        bool selfContained = true;
#ifdef Q_OS_MACOS
        const QString bundle =
                QFileInfo(QCoreApplication::applicationDirPath() + "/../..").canonicalFilePath()
                + '/';
        for (uint32_t index = 0; index < _dyld_image_count(); ++index) {
            const QString path = QString::fromUtf8(_dyld_get_image_name(index));
            const bool system = path.startsWith("/System/Library/") || path.startsWith("/usr/lib/");
            const bool bundled = QFileInfo(path).canonicalFilePath().startsWith(bundle);
            images.append(
                    QJsonObject{ { "path", path }, { "bundled", bundled }, { "system", system } });
            selfContained &= system || bundled;
        }
#elif defined(Q_OS_LINUX)
        const QString package =
                QFileInfo(QCoreApplication::applicationDirPath() + "/..").canonicalFilePath() + '/';
        QStringList paths{ QCoreApplication::applicationFilePath() };
        dl_iterate_phdr(
                [](dl_phdr_info *info, size_t, void *data) {
                    if (info->dlpi_name && info->dlpi_name[0]
                        && QByteArray(info->dlpi_name) != "linux-vdso.so.1"
                        && QByteArray(info->dlpi_name) != "linux-gate.so.1")
                        static_cast<QStringList *>(data)->append(
                                QString::fromUtf8(info->dlpi_name));
                    return 0;
                },
                &paths);
        paths.removeDuplicates();
        for (const auto &path : paths) {
            const QString resolved = QFileInfo(path).canonicalFilePath();
            const QString name = QFileInfo(resolved).fileName();
            const bool bundled = !resolved.isEmpty() && resolved.startsWith(package);
            const bool qtOrMedia = name.startsWith("libQt6") || name.startsWith("libavcodec")
                    || name.startsWith("libavformat") || name.startsWith("libavutil")
                    || name.startsWith("libswresample") || name.startsWith("libswscale")
                    || name.startsWith("libavfilter") || name.startsWith("libavdevice");
            const bool system = !qtOrMedia
                    && (resolved.startsWith("/usr/lib/") || resolved.startsWith("/usr/lib64/")
                        || resolved.startsWith("/lib/") || resolved.startsWith("/lib64/"));
            images.append(QJsonObject{
                    { "path", resolved }, { "bundled", bundled }, { "system", system } });
            selfContained &= bundled || system;
        }
#endif
        QJsonArray resources;
        bool assets = true;
        for (const auto *path :
             { ":/qt/qml/Yalahoo/Music/App.qml", ":/assets/icons/yalahoo/png/yalahoo-32.png",
               ":/assets/branding/logo-light.svg", ":/assets/shaders/probe.frag.qsb",
               ":/assets/shaders/opening-paper.frag.qsb", ":/assets/audio/atmosphere.ogg" }) {
            QFile file(QString::fromUtf8(path));
            const bool valid = file.open(QIODevice::ReadOnly) && file.size() > 0;
            assets &= valid;
            resources.append(
                    QJsonObject{ { "path", QString::fromUtf8(path) }, { "available", valid } });
        }
        const bool policy = !window.isVisible() && verification::windowPolicySatisfied(window);
        const bool passed = loaded && assets && policy && selfContained && qmlWarnings.isEmpty();
        QJsonObject report{
            { "passed", passed },
            { "qtVersion", qVersion() },
            { "platformPlugin", QGuiApplication::platformName() },
            { "qmlInstantiated", true },
            { "qmlWarnings", QJsonArray::fromStringList(qmlWarnings) },
            { "mediaLoadedWithoutOutput", loaded },
            { "mediaError", media.errorString() },
            { "resources", resources },
            { "loadedImages", images },
            { "selfContained", selfContained },
            { "pluginPaths", QJsonArray::fromStringList(app.libraryPaths()) },
            { "qmlPaths",
              QJsonArray::fromStringList(qmlEngine(&window) ? qmlEngine(&window)->importPathList()
                                                            : QStringList()) },
            { "qtPrefix", QLibraryInfo::path(QLibraryInfo::PrefixPath) },
            { "windowPolicy", verification::windowPolicy(window) },
            { "scope",
              "Hidden product QML/resources and real media load; no GPU, playback, device, or presentation claim" }
        };
        QDir().mkpath(QFileInfo(reportPath).absolutePath());
        QSaveFile output(reportPath);
        const bool saved = output.open(QIODevice::WriteOnly)
                && output.write(QJsonDocument(report).toJson()) > 0 && output.commit();
        media.stop();
        media.setSource({ });
        app.exit(passed && saved ? 0 : 24);
    };
    QObject::connect(&media, &QMediaPlayer::mediaStatusChanged, &app, [&](auto status) {
        if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia) {
            loaded = media.hasAudio();
            QTimer::singleShot(0, &media, finish);
        } else if (status == QMediaPlayer::InvalidMedia) {
            QTimer::singleShot(0, &media, finish);
        }
    });
    QObject::connect(&media, &QMediaPlayer::errorOccurred, &media, [&] {
        QTimer::singleShot(0, &media, finish);
    });
    QTimer::singleShot(20000, &media, finish);
    QTimer::singleShot(0, &media, [&] {
        // No audio output is attached, and play() is never called.
        media.setSource(QUrl("qrc:/assets/audio/atmosphere.ogg"));
    });
    return app.exec();
}
