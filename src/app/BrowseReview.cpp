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
            QSaveFile file(output + "/report.json");
            if (!file.open(QIODevice::WriteOnly)) {
                app.exit(5);
                return;
            }
            file.write(QJsonDocument(
                               QJsonObject{ { "passed", passed },
                                            { "captures", captures },
                                            { "windowPolicy", verification::windowPolicy(window) },
                                            { "playbackUnchanged",
                                              library.playbackTrackId() == playback },
                                            { "browseInfo",
                                              QJsonObject::fromVariantMap(library.browseInfo()) },
                                            { "qtVersion", qVersion() } })
                               .toJson());
            app.exit(file.commit() && passed ? 0 : 6);
            return;
        }
        window.resize(sizes[index / 2]);
        scene.lighting()->setNight(index % 2 != 0);
        // Both palette and lighting use the source 650ms transition.
        QTimer::singleShot(850, &app, [&] {
            const auto image = window.grabWindow();
            const QString name = QString("product-%1.png").arg(index, 2, 10, QChar('0'));
            const bool valid = !image.isNull() && image.save(output + '/' + name)
                    && window.size() == sizes[index / 2]
                    && verification::windowPolicySatisfied(window)
                    && window.title() == QGuiApplication::applicationDisplayName();
            captures.append(QJsonObject{ { "file", name },
                                         { "passed", valid },
                                         { "width", window.width() },
                                         { "height", window.height() },
                                         { "dpr", window.devicePixelRatio() } });
            passed &= valid;
            ++index;
            QTimer::singleShot(0, &app, [&] {
                next();
            });
        });
    };
    window.show();
    QTimer::singleShot(300, &app, [&] {
        auto *button = visualItem(window.contentItem(), "nightButton");
        if (!button) {
            qWarning("Browse theme button is missing");
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
    QTimer::singleShot(60000, &app, [&] {
        app.exit(7);
    });
    return app.exec();
}
