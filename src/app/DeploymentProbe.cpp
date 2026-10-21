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

namespace {
// The deployment probe waits only for bounded metadata loading; it never starts song playback.
constexpr auto MediaProbeDeadlineMilliseconds = 20000;

// Nonzero process results are part of the standalone review command protocol.
enum ReviewExit {
    DeploymentVerificationFailed = 24
};
}

using namespace Qt::StringLiterals;

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
                QFileInfo(QCoreApplication::applicationDirPath() + "/../.."_L1).canonicalFilePath()
                + '/'_L1;
        for (uint32_t index = 0; index < _dyld_image_count(); ++index) {
            const QString path = QString::fromUtf8(_dyld_get_image_name(index));
            const bool system =
                    path.startsWith(u"/System/Library/"_s) || path.startsWith(u"/usr/lib/"_s);
            const bool bundled = QFileInfo(path).canonicalFilePath().startsWith(bundle);
            images.append(QJsonObject{
                    { u"path"_s, path }, { u"bundled"_s, bundled }, { u"system"_s, system } });
            selfContained &= system || bundled;
        }
#elif defined(Q_OS_LINUX)
        const QString package =
                QFileInfo(QCoreApplication::applicationDirPath() + "/.."_L1).canonicalFilePath()
                + '/'_L1;
        QStringList paths{ QCoreApplication::applicationFilePath() };
        dl_iterate_phdr(
                [](dl_phdr_info *info, size_t, void *data) {
                    if (info->dlpi_name && info->dlpi_name[0]
                        && QByteArray(info->dlpi_name) != "linux-vdso.so.1"_ba
                        && QByteArray(info->dlpi_name) != "linux-gate.so.1"_ba)
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
            const bool qtOrMedia = name.startsWith(u"libQt6"_s) || name.startsWith(u"libavcodec"_s)
                    || name.startsWith(u"libavformat"_s) || name.startsWith(u"libavutil"_s)
                    || name.startsWith(u"libswresample"_s) || name.startsWith(u"libswscale"_s)
                    || name.startsWith(u"libavfilter"_s) || name.startsWith(u"libavdevice"_s);
            const bool system = !qtOrMedia
                    && (resolved.startsWith(u"/usr/lib/"_s) || resolved.startsWith(u"/usr/lib64/"_s)
                        || resolved.startsWith(u"/lib/"_s) || resolved.startsWith(u"/lib64/"_s));
            images.append(QJsonObject{
                    { u"path"_s, resolved }, { u"bundled"_s, bundled }, { u"system"_s, system } });
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
            resources.append(QJsonObject{ { u"path"_s, QString::fromUtf8(path) },
                                          { u"available"_s, valid } });
        }
        const bool policy = !window.isVisible() && verification::windowPolicySatisfied(window);
        const bool passed = loaded && assets && policy && selfContained && qmlWarnings.isEmpty();
        QJsonObject report{
            { u"passed"_s, passed },
            { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
            { u"platformPlugin"_s, QGuiApplication::platformName() },
            { u"qmlInstantiated"_s, true },
            { u"qmlWarnings"_s, QJsonArray::fromStringList(qmlWarnings) },
            { u"mediaLoadedWithoutOutput"_s, loaded },
            { u"mediaError"_s, media.errorString() },
            { u"resources"_s, resources },
            { u"loadedImages"_s, images },
            { u"selfContained"_s, selfContained },
            { u"pluginPaths"_s, QJsonArray::fromStringList(app.libraryPaths()) },
            { u"qmlPaths"_s,
              QJsonArray::fromStringList(qmlEngine(&window) ? qmlEngine(&window)->importPathList()
                                                            : QStringList()) },
            { u"qtPrefix"_s, QLibraryInfo::path(QLibraryInfo::PrefixPath) },
            { u"windowPolicy"_s, verification::windowPolicy(window) },
            { u"scope"_s,
              u"Hidden product QML/resources and real media load; no GPU, playback, device, or presentation claim"_s }
        };
        QDir().mkpath(QFileInfo(reportPath).absolutePath());
        QSaveFile output(reportPath);
        const bool saved = output.open(QIODevice::WriteOnly)
                && output.write(QJsonDocument(report).toJson()) > 0 && output.commit();
        media.stop();
        media.setSource({ });
        app.exit(passed && saved ? 0 : DeploymentVerificationFailed);
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
    QTimer::singleShot(MediaProbeDeadlineMilliseconds, &media, finish);
    QTimer::singleShot(0, &media, [&] {
        // No audio output is attached, and play() is never called.
        media.setSource(QUrl(u"qrc:/assets/audio/atmosphere.ogg"_s));
    });
    return app.exec();
}
