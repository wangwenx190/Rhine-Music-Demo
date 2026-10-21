#include "FrameClock.h"
#include "LibrarySession.h"
#include <QQuickWindow>
#include "PresentationAudit.h"
#include "OffscreenSurface.h"
#include "PresentationController.h"
#include "DetailModel.h"
#include "OfflineMetadataProvider.h"
#include "LayoutMetrics.h"
#include "VerificationWindow.h"
#include <QDir>
#include <QElapsedTimer>
#include <algorithm>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QQuickItem>
#include <QQmlComponent>
#include <QQmlEngine>
#include "JsonFile.h"
#include <QSignalSpy>
#include <QTest>
#include <atomic>

using namespace Qt::StringLiterals;

using namespace yalahoo;

namespace {
// Event-driven probe waits are bounded independently of injected fixture values.
constexpr int ModelDeadlineMilliseconds = 3000;

// Bounds for asynchronous completion and event delivery; fixture values remain independent.
constexpr int EventPollMilliseconds = 10;

QQuickItem *findVisual(QQuickItem *item, const QString &name)
{
    if (item->objectName() == name)
        return item;
    for (auto *child : item->childItems()) {
        if (auto *found = findVisual(child, name))
            return found;
    }
    return nullptr;
}

auto renderPresentationFrame(QSignalSpy &frames, QSignalSpy &swaps,
                             std::atomic<quint64> &requestedFrame, OffscreenSurface &surface,
                             std::atomic<quint64> &presentedFrame)
{
    const int previous = OffscreenSurface::Enabled ? frames.size() : swaps.size();
    const auto requested = ++requestedFrame;
    surface.requestFrame();
    QElapsedTimer timer;
    timer.start();
    const auto completed = [&] {
        return OffscreenSurface::Enabled ? frames.size() > previous
                                         : presentedFrame.load() >= requested;
    };
    while (!completed() && timer.elapsed() < ModelDeadlineMilliseconds)
        QTest::qWait(EventPollMilliseconds);
    return completed();
}
}
int reviewPresentation(QGuiApplication &, const QString &output)
{
    QDir().mkpath(output);
    OfflineMetadataProvider provider;
    LibrarySession library(&provider);
    LibrarySnapshot snapshot;
    Album album;
    album.id = "anonymous-detail"_L1;
    album.title = u"\u6536\u85cf\u7684\u58f0\u97f3 \u00b7 Anonymous Archive"_s;
    album.artist = u"\u533f\u540d\u97f3\u4e50\u4eba"_s;
    album.description = u"\u79bb\u7ebf\u6536\u85cf\u4e2d\u7684\u4e13\u8f91\u4ecb\u7ecd\u3002"_s;
    album.localNote = u"\u533f\u540d\u672c\u5730\u5907\u6ce8"_s;
    album.introduction = { { u"status"_s, u"uncertain"_s } };
    album.creditsLookup = { { u"status"_s, u"partial"_s },
                            { u"matchedTracks"_s, 1 },
                            { u"totalTracks"_s, 2 } };
    album.descriptionSource = { { u"name"_s, u"\u533f\u540d\u6765\u6e90"_s },
                                { u"url"_s, u"https://example.invalid/album"_s },
                                { u"license"_s, u"\u6d4b\u8bd5\u8f93\u5165"_s } };
    album.producers.append({ u"\u533f\u540d\u5236\u4f5c\u4eba"_s,
                             u"\u5236\u4f5c"_s,
                             u"\u533f\u540d\u6765\u6e90"_s,
                             { },
                             { },
                             { },
                             { } });
    for (int i = 0; i < 10000; ++i) {
        Track track;
        track.id = u"anonymous-track-%1"_s.arg(i);
        track.albumId = album.id;
        track.title = u"\u6536\u85cf\u66f2\u76ee %1 \U0001f3b5"_s.arg(i + 1);
        track.artist = album.artist;
        track.duration = 185;
        track.format = "FLAC"_L1;
        track.discNumber = i < 5000 ? 1 : 2;
        album.tracks.append(track);
    }
    album.producers.append({ u"\u533f\u540d\u5236\u4f5c\u4eba"_s,
                             u"\u7f16\u66f2"_s,
                             u"\u533f\u540d\u6765\u6e90"_s,
                             u"anonymous-track-0"_s,
                             { },
                             u"https://example.invalid/credit/0"_s,
                             { } });
    album.producers.append({ u"\u533f\u540d\u5236\u4f5c\u4eba"_s,
                             u"\u6f14\u594f"_s,
                             u"\u533f\u540d\u6765\u6e90"_s,
                             u"anonymous-track-1"_s,
                             { },
                             u"https://example.invalid/credit/1"_s,
                             { } });
    snapshot.albums.append(album);
    library.setSnapshot(std::move(snapshot));
    library.navigate(album.id);
    DetailModel model;
    LayoutMetrics metrics;
    FrameClock clock;
    PresentationPorts ports;
    ports.archiveReady = [] {
        return true;
    };
    ports.presentationReady = [] {
        return true;
    };
    ports.enterCamera = ports.returnCamera = [] {
    };
    ports.select = ports.switchDetail = [](const PresentationSelection &) {
    };
    PresentationController presentation(&clock, std::move(ports));
    QObject::connect(&presentation, &PresentationController::prepared, &model, [&] {
        model.prepare(library);
    });
    QQmlEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    QQmlComponent component(&engine, QUrl(u"qrc:/qt/qml/Yalahoo/Music/DetailReview.qml"_s));
    std::unique_ptr<QObject> object(component.createWithInitialProperties(
            { { u"metrics"_s, QVariant::fromValue(&metrics) },
              { u"presentation"_s, QVariant::fromValue(&presentation) },
              { u"detailModel"_s, QVariant::fromValue(&model) },
              { u"offscreenRendering"_s, OffscreenSurface::Enabled } }));
    auto *root = qobject_cast<QQuickItem *>(object.get());
    if (!root) {
        qWarning() << component.errors();
        return 3;
    }
    std::atomic<quint64> requestedFrame{ 0 }, synchronizedFrame{ 0 }, presentedFrame{ 0 };
    QQuickRenderControl control;
    QQuickWindow window(OffscreenSurface::Enabled ? &control : nullptr);
    window.setPersistentGraphics(false);
    window.setPersistentSceneGraph(false);
    OffscreenSurface surface(window, control);
    verification::prepareWindow(window, true);
    root->setParentItem(window.contentItem());
    QSignalSpy frames(&surface, &OffscreenSurface::frameRendered);
    QSignalSpy swaps(&window, &QQuickWindow::frameSwapped);
    QObject::connect(
            &window, &QQuickWindow::afterSynchronizing, &window,
            [&] {
                synchronizedFrame.store(requestedFrame.load());
            },
            Qt::DirectConnection);
    QObject::connect(
            &window, &QQuickWindow::frameSwapped, &window,
            [&] {
                presentedFrame.store(synchronizedFrame.load());
            },
            Qt::DirectConnection);
    const auto render = [&] {
        return renderPresentationFrame(frames, swaps, requestedFrame, surface, presentedFrame);
    };
    bool passed = true;
    QJsonArray captures, lifecycle;
    const auto bounds = [](QQuickItem *item, QQuickItem *origin) {
        if (!item)
            return QJsonObject{ };
        const auto point = item->mapToItem(origin, { 0, 0 });
        return QJsonObject{ { u"x"_s, point.x() },
                            { u"y"_s, point.y() },
                            { u"width"_s, item->width() },
                            { u"height"_s, item->height() },
                            { u"fontSize"_s, item->property("cssPixelSize").toDouble() } };
    };
    const QList<QSize> sizes{ { 1920, 1080 }, { 1280, 720 },  { 995, 858 },
                              { 960, 900 },   { 930, 900 },   { 390, 844 },
                              { 320, 720 },   { 2560, 1080 }, { 3440, 1440 } };
    surface.show();
    presentation.setReducedMotion(true);
    const QList<double> dprs = OffscreenSurface::Enabled
            ? QList<double>{ 1, 1.5, 2 }
            : QList<double>{ window.devicePixelRatio() };
    for (const auto dpr : dprs) {
        if (OffscreenSurface::Enabled)
            surface.setDevicePixelRatio(dpr);
        for (const auto &size : sizes) {
            window.resize(size);
            root->setSize(size);
            metrics.setViewport(size.width(), size.height());
            for (bool night : { false, true }) {
                root->setProperty("night", night);
                presentation.reset();
                presentation.open();
                passed = render() && passed;
                presentation.update();
                passed = render() && passed;
                auto *list = root->findChild<QQuickItem *>("detailTrackList");
                auto *title = root->findChild<QQuickItem *>("detailTitle");
                auto *page = root->findChild<QQuickItem *>("detailPage");
                if (!list || !title || !page)
                    return 4;
                const double expectedLeft = metrics.portrait()
                        ? std::clamp(size.width() * .04, 22.0, 42.0)
                        : size.width() * .52;
                const double expectedTop = metrics.portrait() ? size.height() * .5
                        : size.width() >= 1700                ? 210
                        : size.width() <= 1100                ? 132
                                                              : 170;
                const double titleY = title->mapToScene({ 0, 0 }).y();
                const bool geometry = std::abs(list->x() - expectedLeft) <= 1
                        && std::abs(list->y() - expectedTop) <= 1 && list->width() > 0
                        && list->height() > 0 && title->property("text").toString() == album.title
                        && titleY >= expectedTop && titleY - expectedTop < 60;
                passed = geometry && presentation.detailEnabled() && page->isEnabled() && passed;
                const auto image = surface.grab();
                const auto file = u"detail-%1x%2-%3-dpr%4.png"_s.arg(size.width())
                                          .arg(size.height())
                                          .arg(night ? "night"_L1 : "day"_L1)
                                          .arg(dpr);
                passed = !image.isNull() && image.save(QDir(output).filePath(file)) && passed;
                auto find = [&](const QString &name) {
                    return findVisual(root, name);
                };
                QJsonObject fields{ { u"article"_s, bounds(list, root) },
                                    { u"caption"_s, bounds(find(u"detailCaption"_s), root) },
                                    { u"back"_s, bounds(find(u"detailBack"_s), root) },
                                    { u"title"_s, bounds(title, list) },
                                    { u"artist"_s, bounds(find(u"detailArtist"_s), list) },
                                    { u"facts"_s, bounds(find(u"detailFacts"_s), list) },
                                    { u"factLabel"_s, bounds(find(u"detailFactLabel"_s), list) },
                                    { u"factValue"_s, bounds(find(u"detailFactValue"_s), list) },
                                    { u"tabs"_s, bounds(find(u"detailTabs"_s), list) },
                                    { u"trackTitle"_s, bounds(find(u"detailTrackTitle"_s), list) },
                                    { u"trackArtist"_s,
                                      bounds(find(u"detailTrackArtist"_s), list) } };
                // QML list width excludes CSS padding; the scroll box includes it.
                auto article = fields[u"article"_s].toObject();
                article[u"width"_s] = list->width() + (metrics.portrait() ? 5 : 9);
                fields[u"article"_s] = article;
                auto facts = fields[u"facts"_s].toObject();
                facts[u"gap"_s] = find(u"detailFacts"_s)->property("columnSpacing").toDouble();
                facts[u"rowGap"_s] = find(u"detailFacts"_s)->property("rowSpacing").toDouble();
                // CSS facts includes its two separately drawn borders.
                facts[u"y"_s] = facts[u"y"_s].toDouble() - 1;
                facts[u"height"_s] = facts[u"height"_s].toDouble() + 2;
                fields[u"facts"_s] = facts;
                captures.append(QJsonObject{ { u"fields"_s, fields },
                                             { u"width"_s, size.width() },
                                             { u"height"_s, size.height() },
                                             { u"night"_s, night },
                                             { u"geometryPassed"_s, geometry },
                                             { u"listX"_s, list->x() },
                                             { u"listY"_s, list->y() },
                                             { u"titleSceneY"_s, titleY },
                                             { u"expectedTop"_s, expectedTop },
                                             { u"image"_s, file },
                                             { u"dpr"_s, window.effectiveDevicePixelRatio() } });
                presentation.back();
                passed = !page->isEnabled() && passed;
                presentation.update();
            }
        }
    }
    // A large jump must instantiate only a bounded visible/reuse pool.
    presentation.open();
    passed = render() && passed;
    presentation.update();
    passed = render() && passed;
    auto *list = root->findChild<QQuickItem *>("detailTrackList");
    passed = QMetaObject::invokeMethod(list, "positionViewAtIndex", Q_ARG(int, 8000), Q_ARG(int, 0))
            && passed;
    passed = render() && passed;
    int delegates = 0;
    const auto visit = [&](auto &&self, QQuickItem *item) -> void {
        if (item->objectName() == "trackDelegate"_L1)
            ++delegates;
        for (auto *child : item->childItems())
            self(self, child);
    };
    visit(visit, list);
    passed = delegates > 0 && delegates < 100 && model.rowCount() == 10000
            && library.navigationAlbumId() == album.id && library.playbackTrackId().isEmpty()
            && passed;
    auto *page = root->findChild<QQuickItem *>("detailPage");
    auto *tabs = root->findChild<QQuickItem *>("detailTabs");
    auto *about = root->findChild<QQuickItem *>("detailAboutTab");
    auto *trackTab = root->findChild<QQuickItem *>("detailTracksTab");
    const bool sticky = tabs && std::abs(tabs->y()) <= 1;
    const auto click = [&](QQuickItem *item) {
        if (!item)
            return false;
        const auto point = item->mapToScene({ item->width() / 2, item->height() / 2 }).toPoint();
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, point);
        return render();
    };
    const bool switched = click(about) && page->property("activeTab").toInt() == 1;
    const auto *aboutView = list->property("currentView").value<QObject *>();
    const double actualScroll =
            aboutView->property("contentY").toDouble() - aboutView->property("originY").toDouble();
    const double expectedScroll =
            std::min(list->property("savedScroll").toDouble(),
                     std::max(0.0,
                              aboutView->property("contentHeight").toDouble()
                                      - aboutView->property("height").toDouble()));
    const bool stayedAtTabs = std::abs(actualScroll - expectedScroll) <= 1;
    const bool switchedBack = click(trackTab) && page->property("activeTab").toInt() == 0;
    QJsonObject interaction{ { u"sticky"_s, sticky },
                             { u"switched"_s, switched },
                             { u"keptScroll"_s, stayedAtTabs },
                             { u"switchedBack"_s, switchedBack } };
    passed = sticky && switched && stayedAtTabs && switchedBack && passed;
    QMetaObject::invokeMethod(list, "positionViewAtBeginning");
    passed = render() && passed;
    QSignalSpy trackRequests(page, SIGNAL(trackRequested(QString)));
    const bool trackInput = click(findVisual(root, u"detailTrackTitle"_s))
            && trackRequests.size() == 1 && trackRequests[0][0].toString() == "anonymous-track-0"_L1
            && library.playbackTrackId().isEmpty();
    QMetaObject::invokeMethod(page, "setTab", Q_ARG(int, 1));
    passed = render() && passed;
    const bool keyboardStart = page->property("activeTab").toInt() == 1;
    about->forceActiveFocus(Qt::OtherFocusReason);
    QTest::keyClick(&window, Qt::Key_Left);
    const bool keyboard = render() && keyboardStart && page->property("activeTab").toInt() == 0;
    interaction[u"trackIntentOnly"_s] = trackInput;
    interaction[u"keyboardTab"_s] = keyboard;
    passed = trackInput && keyboard && passed;
    QMetaObject::invokeMethod(page, "setTab", Q_ARG(int, 1));
    passed = render() && passed;
    const auto *introStatus = findVisual(root, u"detailIntroductionStatus"_s);
    const auto *creditStatus = findVisual(root, u"detailCreditsStatus"_s);
    const bool localData = introStatus && creditStatus
            && introStatus->property("text").toString().contains(u"\u5b58\u7591"_s)
            && creditStatus->property("text").toString().contains(u"1 / 2"_s);
    interaction[u"offlineLookupReadable"_s] = localData;
    auto *expand = findVisual(root, u"detailCreditExpand"_s);
    const bool creditsExpanded =
            expand && click(expand) && expand->parentItem()->property("expanded").toBool();
    interaction[u"creditExpansion"_s] = creditsExpanded;
    passed = localData && creditsExpanded && passed;
    album.offline = true;
    LibrarySnapshot offlineSnapshot;
    offlineSnapshot.albums.append(album);
    library.setSnapshot(std::move(offlineSnapshot));
    library.navigate(album.id);
    model.prepare(library);
    QMetaObject::invokeMethod(page, "setTab", Q_ARG(int, 0));
    QMetaObject::invokeMethod(list, "positionViewAtBeginning");
    passed = render() && passed;
    const auto *badge = findVisual(root, u"detailOfflineBadge"_s);
    const bool offlineInput = badge && badge->isVisible()
            && click(findVisual(root, u"detailTrackTitle"_s)) && trackRequests.size() == 1;
    interaction[u"offlineBadgeAndDisabledTrack"_s] = offlineInput;
    passed = offlineInput && passed;
    presentation.setReducedMotion(false);
    presentation.back();
    passed = !page->isEnabled() && page->isVisible() && passed;
    const auto exitRevision = presentation.layoutRevision();
    presentation.open();
    presentation.detailSurface()->finish();
    presentation.update();
    presentation.browseSurface()->finish();
    passed = render() && passed;
    presentation.layoutReady(exitRevision); // Old layout must not unlock the replacement.
    presentation.update();
    passed = render() && passed;
    presentation.setReducedMotion(true);
    presentation.update();
    passed = render() && passed;
    passed = presentation.detailEnabled() && warnings.isEmpty() && passed;
    for (int cycle = 0; cycle < 10; ++cycle) {
        surface.hide();
        window.releaseResources();
        presentation.reset();
        surface.show();
        presentation.open();
        const bool rendered = render();
        presentation.update();
        const bool restored = render() && rendered && presentation.detailEnabled();
        lifecycle.append(QJsonObject{ { u"cycle"_s, cycle }, { u"passed"_s, restored } });
        passed = restored && passed;
    }
    // A fully clipped tab must retire its local sampling layers, including small landscape windows.
    window.resize(320, 240);
    root->setSize({ 320, 240 });
    metrics.setViewport(320, 240);
    passed = render() && render() && passed;
    const auto *blurLoader = findVisual(root, u"detailTabsBlurLoader"_s);
    const bool clippedBackdropRetired = blurLoader && !blurLoader->property("active").toBool();
    interaction[u"clippedBackdropRetired"_s] = clippedBackdropRetired;
    passed = clippedBackdropRetired && passed;
    surface.hide();
    root->setParentItem(nullptr);
    const auto policy = verification::windowPolicy(window);
    passed = verification::windowPolicySatisfied(window) && passed;
    QJsonObject report{ { u"passed"_s, passed },
                        { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
                        { u"platform"_s, QSysInfo::productType() },
                        { u"configuration"_s, u"Release"_s },
                        { u"offscreen"_s, OffscreenSurface::Enabled },
                        { u"graphicsApi"_s, int(window.rendererInterface()->graphicsApi()) },
                        { u"captures"_s, captures },
                        { u"interaction"_s, interaction },
                        { u"trackCount"_s, model.rowCount() },
                        { u"lifecycle"_s, lifecycle },
                        { u"delegates"_s, delegates },
                        { u"qmlWarnings"_s, warnings.size() },
                        { u"windowPolicy"_s, policy },
                        { u"resourcePlan"_s, surface.diagnostics() },
                        { u"presentOrVsyncVerified"_s, false } };
    if (!yalahoo::writeJsonFile(QDir(output).filePath(u"report.json"_s), QJsonDocument(report)))
        return 5;
    return passed ? 0 : 6;
}
