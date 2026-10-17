#include "Logging.h"
#include "BrowseReview.h"
#include "LibrarySession.h"
#include "SceneAdapter.h"
#include "VerificationWindow.h"
#include <QGuiApplication>
#include <QQuickWindow>
#include <QQuickItem>
#include <QMouseEvent>
#include <QDir>
#include <QTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <functional>

using namespace Qt::StringLiterals;

namespace {
QQuickItem *visualItem(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name)
        return root;
    for (auto *child : root->childItems()) {
        if (auto *found = visualItem(child, name))
            return found;
    }
    return nullptr;
}
}

int yalahoo::reviewBrowse(QGuiApplication &app, QQuickWindow &window, SceneAdapter &scene,
                          LibrarySession &library, const QString &output)
{
    QObject reviewLifetime;
    QDir().mkpath(output);
    const QList<QSize> sizes{ { 1920, 1080 }, { 1280, 720 },  { 995, 858 },
                              { 960, 900 },   { 930, 900 },   { 390, 844 },
                              { 320, 720 },   { 2560, 1080 }, { 3440, 1440 } };
    const auto playback = library.playbackTrackId();
    QJsonArray captures;
    int index = 0;
    bool passed = true;
    std::function<void()> next;
    next = [&] {
        if (index == sizes.size() * 2) {
            const auto *info = window.findChild<QQuickItem *>("browseInfo");
            passed &= info && !library.browseInfo().isEmpty()
                    && library.playbackTrackId() == playback
                    && verification::windowPolicySatisfied(window);
            QSaveFile file(output + "/report.json"_L1);
            if (!file.open(QIODevice::WriteOnly)) {
                app.exit(5);
                return;
            }
            file.write(
                    QJsonDocument(
                            QJsonObject{ { u"passed"_s, passed },
                                         { u"captures"_s, captures },
                                         { u"windowPolicy"_s, verification::windowPolicy(window) },
                                         { u"playbackUnchanged"_s,
                                           library.playbackTrackId() == playback },
                                         { u"browseInfo"_s,
                                           QJsonObject::fromVariantMap(library.browseInfo()) },
                                         { u"qtVersion"_s, QString::fromUtf8(qVersion()) } })
                            .toJson());
            app.exit(file.commit() && passed ? 0 : 6);
            return;
        }
        window.resize(sizes[index / 2]);
        scene.lighting()->setNight(index % 2 != 0);
        // Both palette and lighting use the source 650ms transition.
        QTimer::singleShot(850, &reviewLifetime, [&] {
            const auto image = window.grabWindow();
            const QString name = u"product-%1.png"_s.arg(index, 2, 10, '0'_L1);
            const bool valid = !image.isNull() && image.save(output + '/'_L1 + name)
                    && window.size() == sizes[index / 2]
                    && verification::windowPolicySatisfied(window)
                    && window.title() == QGuiApplication::applicationDisplayName();
            captures.append(QJsonObject{ { u"file"_s, name },
                                         { u"passed"_s, valid },
                                         { u"width"_s, window.width() },
                                         { u"height"_s, window.height() },
                                         { u"dpr"_s, window.devicePixelRatio() } });
            passed &= valid;
            ++index;
            QTimer::singleShot(0, &reviewLifetime, [&] {
                next();
            });
        });
    };
    window.show();
    QTimer::singleShot(300, &reviewLifetime, [&] {
        auto *button = visualItem(window.contentItem(), u"nightButton"_s);
        if (!button) {
            qCWarning(yalahoo::logApp, "Browse theme button is missing");
            app.exit(6);
            return;
        }
        const auto point = button->mapToScene({ button->width() / 2, button->height() / 2 });
        QMouseEvent press(QEvent::MouseButtonPress, point, point, Qt::LeftButton, Qt::LeftButton,
                          Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, point, point, Qt::LeftButton, Qt::NoButton,
                            Qt::NoModifier);
        QCoreApplication::sendEvent(&window, &press);
        QCoreApplication::sendEvent(&window, &release);
        passed &= scene.lighting()->night();
        next();
    });
    QTimer::singleShot(60000, &reviewLifetime, [&] {
        app.exit(7);
    });
    return app.exec();
}
