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
#include <QSaveFile>
#include <QSignalSpy>
#include <QTest>
#include <atomic>

using namespace yalahoo;
int reviewPresentation(QGuiApplication &, const QString &output)
{
    QDir().mkpath(output);
    OfflineMetadataProvider provider;
    LibrarySession library(&provider);
    LibrarySnapshot snapshot;
    Album album;
    album.id = "anonymous-detail";
    album.title = "收藏的声音 · Anonymous Archive";
    album.artist = "匿名音乐人";
    album.description = "离线收藏中的专辑介绍。";
    album.localNote = "匿名本地备注";
    album.descriptionSource = { { "name", "匿名来源" },
                                { "url", "https://example.invalid/album" },
                                { "license", "测试输入" } };
    album.producers.append({ "匿名制作人", "制作", "匿名来源", { }, { }, { }, { } });
    for (int i = 0; i < 10000; ++i) {
        Track track;
        track.id = QStringLiteral("anonymous-track-%1").arg(i);
        track.albumId = album.id;
        track.title = QStringLiteral("收藏曲目 %1 🎵").arg(i + 1);
        track.artist = album.artist;
        track.duration = 185;
        track.format = "FLAC";
        track.discNumber = i < 5000 ? 1 : 2;
        album.tracks.append(track);
    }
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
    QQmlComponent component(&engine, QUrl("qrc:/qt/qml/Yalahoo/Music/DetailReview.qml"));
    std::unique_ptr<QObject> object(component.createWithInitialProperties(
            { { "metrics", QVariant::fromValue(&metrics) },
              { "presentation", QVariant::fromValue(&presentation) },
              { "detailModel", QVariant::fromValue(&model) },
              { "offscreenRendering", OffscreenSurface::Enabled } }));
    auto *root = qobject_cast<QQuickItem *>(object.get());
    if (!root) {
        qWarning() << component.errors();
        return 3;
    }
    QQuickRenderControl control;
    QQuickWindow window(OffscreenSurface::Enabled ? &control : nullptr);
    window.setPersistentGraphics(false);
    window.setPersistentSceneGraph(false);
    OffscreenSurface surface(window, control);
    verification::prepareWindow(window, true);
    root->setParentItem(window.contentItem());
    QSignalSpy frames(&surface, &OffscreenSurface::frameRendered);
    QSignalSpy swaps(&window, &QQuickWindow::frameSwapped);
    std::atomic<quint64> requestedFrame{ 0 }, synchronizedFrame{ 0 }, presentedFrame{ 0 };
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
        const int previous = OffscreenSurface::Enabled ? frames.size() : swaps.size();
        const auto requested = ++requestedFrame;
        surface.requestFrame();
        QElapsedTimer timer;
        timer.start();
        const auto completed = [&] {
            return OffscreenSurface::Enabled ? frames.size() > previous
                                             : presentedFrame.load() >= requested;
        };
        while (!completed() && timer.elapsed() < 3000)
            QTest::qWait(10);
        return completed();
    };
    bool passed = true;
    QJsonArray captures, lifecycle;
    const QList<QSize> sizes{ { 1920, 1080 }, { 1280, 720 },  { 995, 858 },
                              { 960, 900 },   { 930, 900 },   { 390, 844 },
                              { 320, 720 },   { 2560, 1080 }, { 3440, 1440 } };
    surface.show();
    presentation.setReducedMotion(true);
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
            const auto file = QStringLiteral("detail-%1x%2-%3.png")
                                      .arg(size.width())
                                      .arg(size.height())
                                      .arg(night ? "night" : "day");
            passed = !image.isNull() && image.save(QDir(output).filePath(file)) && passed;
            captures.append(QJsonObject{ { "width", size.width() },
                                         { "height", size.height() },
                                         { "night", night },
                                         { "geometryPassed", geometry },
                                         { "listX", list->x() },
                                         { "listY", list->y() },
                                         { "titleSceneY", titleY },
                                         { "expectedTop", expectedTop },
                                         { "image", file },
                                         { "dpr", window.devicePixelRatio() } });
            presentation.back();
            passed = !page->isEnabled() && passed;
            presentation.update();
        }
    }
    // A large jump must instantiate only a bounded visible/reuse pool.
    presentation.open();
    passed = render() && passed;
    presentation.update();
    passed = render() && passed;
    auto *list = root->findChild<QQuickItem *>("detailTrackList");
    passed = QMetaObject::invokeMethod(list, "positionViewAtIndex", Q_ARG(QVariant, 8000),
                                       Q_ARG(QVariant, 0))
            && passed;
    passed = render() && passed;
    int delegates = 0;
    const auto visit = [&](auto &&self, QQuickItem *item) -> void {
        if (item->objectName() == "trackDelegate")
            ++delegates;
        for (auto *child : item->childItems())
            self(self, child);
    };
    visit(visit, list);
    passed = delegates > 0 && delegates < 100 && model.rowCount() == 10000
            && library.navigationAlbumId() == album.id && library.playbackTrackId().isEmpty()
            && passed;
    auto *page = root->findChild<QQuickItem *>("detailPage");
    page->setProperty("activeTab", 1);
    passed = render() && passed;
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
        lifecycle.append(QJsonObject{ { "cycle", cycle }, { "passed", restored } });
        passed = restored && passed;
    }
    surface.hide();
    root->setParentItem(nullptr);
    const auto policy = verification::windowPolicy(window);
    passed = verification::windowPolicySatisfied(window) && passed;
    QJsonObject report{ { "passed", passed },
                        { "qtVersion", qVersion() },
                        { "platform", QSysInfo::productType() },
                        { "configuration", "Release" },
                        { "offscreen", OffscreenSurface::Enabled },
                        { "graphicsApi", int(window.rendererInterface()->graphicsApi()) },
                        { "captures", captures },
                        { "trackCount", model.rowCount() },
                        { "lifecycle", lifecycle },
                        { "delegates", delegates },
                        { "qmlWarnings", warnings.size() },
                        { "windowPolicy", policy },
                        { "resourcePlan", surface.diagnostics() },
                        { "presentOrVsyncVerified", false } };
    QSaveFile file(QDir(output).filePath("report.json"));
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(report).toJson()) < 0
        || !file.commit())
        return 5;
    return passed ? 0 : 6;
}
