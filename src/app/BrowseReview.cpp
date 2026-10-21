#include "ReviewSupport.h"
#include "Units.h"
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
// Probe settling delays and deadline are distinct from the explicit viewport fixture matrix.
constexpr auto CaptureSettleMilliseconds = 850;
constexpr auto ResizeSettleMilliseconds = 300;

// Nonzero process results are part of the standalone review command protocol.
enum ReviewExit {
    CaptureFailed = 5,
    ReportFailed = 6,
    ReviewTimedOut = 7
};

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

namespace yalahoo {
namespace {
// Shared references remain valid until this review event loop has stopped.
struct BrowseReviewState final
{
    int &index;
    const QList<QSize> &sizes;
    QQuickWindow &window;
    bool &passed;
    LibrarySession &library;
    const QString &playback;
    const QString &output;
    QGuiApplication &app;
    QJsonArray &captures;
    SceneAdapter &scene;
    QObject &reviewLifetime;
    std::function<void()> &next;
};
auto advanceBrowseCapture(BrowseReviewState &state)
{
    if (state.index == state.sizes.size() * 2) {
        const auto *info = state.window.findChild<QQuickItem *>("browseInfo");
        state.passed &= info && !state.library.browseInfo().isEmpty()
                && state.library.playbackTrackId() == state.playback
                && verification::windowPolicySatisfied(state.window);
        QSaveFile file(state.output + "/report.json"_L1);
        if (!file.open(QIODevice::WriteOnly)) {
            state.app.exit(CaptureFailed);
            return;
        }
        file.write(QJsonDocument(
                           QJsonObject{
                                   { u"passed"_s, state.passed },
                                   { u"captures"_s, state.captures },
                                   { u"windowPolicy"_s, verification::windowPolicy(state.window) },
                                   { u"playbackUnchanged"_s,
                                     state.library.playbackTrackId() == state.playback },
                                   { u"browseInfo"_s,
                                     QJsonObject::fromVariantMap(state.library.browseInfo()) },
                                   { u"qtVersion"_s, QString::fromUtf8(qVersion()) } })
                           .toJson());
        state.app.exit(file.commit() && state.passed ? 0 : ReportFailed);
        return;
    }
    state.window.resize(state.sizes[state.index / 2]);
    state.scene.lighting()->setNight(state.index % 2 != 0);
    // Both palette and lighting use the source 650ms transition.
    QTimer::singleShot(CaptureSettleMilliseconds, &state.reviewLifetime, [&] {
        const auto image = state.window.grabWindow();
        const QString name =
                u"product-%1.png"_s.arg(state.index, 2, yalahoo::units::DecimalBase, '0'_L1);
        const bool valid = !image.isNull() && image.save(state.output + '/'_L1 + name)
                && state.window.size() == state.sizes[state.index / 2]
                && verification::windowPolicySatisfied(state.window)
                && state.window.title() == QGuiApplication::applicationDisplayName();
        state.captures.append(QJsonObject{ { u"file"_s, name },
                                           { u"passed"_s, valid },
                                           { u"width"_s, state.window.width() },
                                           { u"height"_s, state.window.height() },
                                           { u"dpr"_s, state.window.devicePixelRatio() } });
        state.passed &= valid;
        ++state.index;
        QTimer::singleShot(0, &state.reviewLifetime, state.next);
    });
}

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
    BrowseReviewState state{ index,  sizes, window,   passed, library,        playback,
                             output, app,   captures, scene,  reviewLifetime, next };
    next = [&] {
        return advanceBrowseCapture(state);
    };
    window.show();
    QTimer::singleShot(ResizeSettleMilliseconds, &reviewLifetime, [&] {
        auto *button = visualItem(window.contentItem(), u"nightButton"_s);
        if (!button) {
            qCWarning(yalahoo::logApp, "Browse theme button is missing");
            app.exit(ReportFailed);
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
    QTimer::singleShot(yalahoo::review::DefaultDeadlineMilliseconds, &reviewLifetime, [&] {
        app.exit(ReviewTimedOut);
    });
    return app.exec();
}
