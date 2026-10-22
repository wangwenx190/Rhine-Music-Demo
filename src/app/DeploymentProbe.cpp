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
#include "JsonFile.h"
#include <QTimer>

using namespace Qt::StringLiterals;

#ifdef Q_OS_MACOS
#  include <mach-o/dyld.h>
#elif defined(Q_OS_LINUX)
#  include <link.h>
#endif

namespace yalahoo {
namespace {
// The deployment probe waits only for bounded metadata loading; it never starts song playback.
constexpr auto MediaProbeDeadlineMilliseconds = 20000;

// Nonzero process results are part of the standalone review command protocol.
enum ReviewExit {
    DeploymentVerificationFailed = 24
};

// Shared references remain valid until this review event loop has stopped.
struct DeploymentState final
{
    bool &finished;
    QQuickWindow &window;
    bool &loaded;
    const QStringList &qmlWarnings;
    QMediaPlayer &media;
    QGuiApplication &app;
    const QString &reportPath;
};
auto finishDeploymentProbe(DeploymentState &state)
{
    if (state.finished)
        return;
    state.finished = true;
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
                    static_cast<QStringList *>(data)->append(QString::fromUtf8(info->dlpi_name));
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
           ":/assets/branding/logo-light.svg", ":/assets/shaders/reel-slot.frag.qsb",
           ":/assets/shaders/opening-paper.frag.qsb", ":/assets/audio/atmosphere.ogg" }) {
        QFile file(QString::fromUtf8(path));
        const bool valid = file.open(QIODevice::ReadOnly) && file.size() > 0;
        assets &= valid;
        resources.append(
                QJsonObject{ { u"path"_s, QString::fromUtf8(path) }, { u"available"_s, valid } });
    }
    const bool policy =
            !state.window.isVisible() && verification::windowPolicySatisfied(state.window);
    const bool passed =
            state.loaded && assets && policy && selfContained && state.qmlWarnings.isEmpty();
    QJsonObject report{
        { u"passed"_s, passed },
        { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
        { u"platformPlugin"_s, QGuiApplication::platformName() },
        { u"qmlInstantiated"_s, true },
        { u"qmlWarnings"_s, QJsonArray::fromStringList(state.qmlWarnings) },
        { u"mediaLoadedWithoutOutput"_s, state.loaded },
        { u"mediaError"_s, state.media.errorString() },
        { u"resources"_s, resources },
        { u"loadedImages"_s, images },
        { u"selfContained"_s, selfContained },
        { u"pluginPaths"_s, QJsonArray::fromStringList(state.app.libraryPaths()) },
        { u"qmlPaths"_s,
          QJsonArray::fromStringList(qmlEngine(&state.window)
                                             ? qmlEngine(&state.window)->importPathList()
                                             : QStringList()) },
        { u"qtPrefix"_s, QLibraryInfo::path(QLibraryInfo::PrefixPath) },
        { u"windowPolicy"_s, verification::windowPolicy(state.window) },
        { u"scope"_s,
          u"Hidden product QML/resources and real media load; no GPU, playback, device, or presentation claim"_s }
    };
    QDir().mkpath(QFileInfo(state.reportPath).absolutePath());
    const bool saved = yalahoo::writeJsonFile(state.reportPath, QJsonDocument(report));
    state.media.stop();
    state.media.setSource({ });
    state.app.exit(passed && saved ? 0 : DeploymentVerificationFailed);
}
}

int verifyDeployment(QGuiApplication &app, QQuickWindow &window, const QString &reportPath,
                     const QStringList &qmlWarnings)
{
    QMediaPlayer media;
    bool finished = false;
    bool loaded = false;
    DeploymentState state{ finished, window, loaded, qmlWarnings, media, app, reportPath };
    const auto finish = [&] {
        return finishDeploymentProbe(state);
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
}
