#include "LibraryAudit.h"
#include "OffscreenSurface.h"
#include "DetailPorts.h"
#include "DetailModel.h"
#include "TrackFocusController.h"
#include "PanelController.h"
#include "SceneAdapter.h"
#include "SceneItem.h"
#include "OverviewController.h"
#include "InputRouter.h"
#include "RulerController.h"
#include "OfflineMetadataProvider.h"
#include "VerificationWindow.h"
#include <QDir>
#include <QFile>
#include <QCryptographicHash>
#include <QSysInfo>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTest>
#include <atomic>

using namespace yalahoo;
namespace {
QQuickItem *visual(QQuickItem *item, const QString &name)
{
    if (item->objectName() == name)
        return item;
    for (auto *child : item->childItems())
        if (auto *found = visual(child, name))
            return found;
    return nullptr;
}
}
int reviewLibrary(QGuiApplication &, const QString &output)
{
    QDir().mkpath(output);
    MotionController motion;
    SelectionController selection(&motion);
    SceneAdapter scene(motion.clock());
    OfflineMetadataProvider provider;
    LibrarySession library(&provider);
    LibrarySnapshot snapshot;
    snapshot.genres = { { "g", "流派一", { }, { }, { } }, { "h", "流派二", { }, { }, { } } };
    for (int i = 0; i < 20; ++i) {
        Album album;
        album.id = QString("album-%1").arg(i);
        album.title = QString("收藏专辑 %1 · Anonymous Archive").arg(i);
        album.artist = i < 10 ? "Artist A" : "Artist B";
        album.genreId = i < 10 ? "g" : "h";
        for (int j = 0; j < 200; ++j) {
            Track track;
            track.id = QString("track-%1").arg(j);
            track.albumId = album.id;
            track.title = QString("Song %1 · 收藏曲目 🎵").arg(j);
            track.artist = album.artist;
            track.duration = 185;
            track.discNumber = j < 100 ? 1 : 2;
            album.tracks.append(track);
        }
        snapshot.albums.append(album);
    }
    library.setSnapshot(snapshot);
    library.sort("artist");
    selection.setColumns(library.albums()->order().columns, library.snapshot());
    scene.setSelection(&selection);
    library.navigate(selection.albumId());
    library.setPlaybackIds("album-0", "track-0");
    QObject::connect(&selection, &SelectionController::selectionChanged, &library, [&] {
        library.navigate(selection.albumId());
    });
    PresentationController presentation(motion.clock(), detailPorts(scene, selection, library));
    DetailModel detail;
    QObject::connect(&presentation, &PresentationController::prepared, &detail, [&] {
        detail.prepare(library);
    });
    TrackFocusController focus(&library, &presentation, motion.clock());
    PanelController panels(motion.clock());
    InputRouter input(&selection);
    RulerController ruler(&motion, &selection);
    OverviewController overview(&motion);
    overview.bind(&scene, &selection, &presentation);
    QObject::connect(&focus, &TrackFocusController::navigationRequested, &overview, [&] {
        overview.setActive(false);
    });
    const auto updateInput = [&] {
        input.setEnabled(presentation.archiveEnabled() && !panels.surface()->shown());
    };
    QObject::connect(panels.surface(), &SurfaceTransition::changed, &input, updateInput);
    QObject::connect(&presentation, &PresentationController::changed, &input, updateInput);
    QQmlEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    QQmlComponent component(&engine, QUrl("qrc:/qt/qml/Yalahoo/Music/LibraryReview.qml"));
    std::unique_ptr<QObject> object(component.createWithInitialProperties(
            { { "librarySession", QVariant::fromValue(&library) },
              { "motionController", QVariant::fromValue(&motion) },
              { "sceneAdapter", QVariant::fromValue(&scene) },
              { "presentationController", QVariant::fromValue(&presentation) },
              { "detailModel", QVariant::fromValue(&detail) },
              { "trackFocusController", QVariant::fromValue(&focus) },
              { "panelController", QVariant::fromValue(&panels) },
              { "inputRouter", QVariant::fromValue(&input) },
              { "rulerController", QVariant::fromValue(&ruler) },
              { "overviewController", QVariant::fromValue(&overview) } }));
    auto *root = qobject_cast<QQuickItem *>(object.get());
    if (!root) {
        qWarning() << component.errors();
        return 3;
    }
    QQuickRenderControl control;
    QQuickWindow window(OffscreenSurface::Enabled ? &control : nullptr);
    verification::prepareWindow(window, true);
    OffscreenSurface surface(window, control);
    root->setParentItem(window.contentItem());
    const auto deviceCaptured = std::make_shared<std::atomic_bool>(false);
    QObject::connect(
            &window, &QQuickWindow::afterRendering, &window,
            [&window, &control, deviceCaptured] {
                if (deviceCaptured->exchange(true))
                    return;
                auto *api = OffscreenSurface::Enabled
                        ? control.rhi()
                        : static_cast<QRhi *>(window.rendererInterface()->getResource(
                                  &window, QSGRendererInterface::RhiResource));
                const QVariantMap info{
                    { "backend", api ? QString::fromLatin1(api->backendName()) : QString() },
                    { "gpu", api ? QString::fromUtf8(api->driverInfo().deviceName) : QString() }
                };
                QMetaObject::invokeMethod(
                        &window,
                        [&window, info] {
                            window.setProperty("libraryAuditDevice", info);
                        },
                        Qt::QueuedConnection);
            },
            Qt::DirectConnection);
    input.attach(&window);
    QSignalSpy rendered(&surface, &OffscreenSurface::frameRendered);
    QSignalSpy swapped(&window, &QQuickWindow::frameSwapped);
    QSignalSpy playback(&library, &LibrarySession::playbackChanged);
    const auto render = [&] {
        const int before = OffscreenSurface::Enabled ? rendered.size() : swapped.size();
        surface.requestFrame();
        QElapsedTimer wait;
        wait.start();
        while ((OffscreenSurface::Enabled ? rendered.size() : swapped.size()) == before
               && wait.elapsed() < 5000)
            QTest::qWait(5);
        return (OffscreenSurface::Enabled ? rendered.size() : swapped.size()) > before;
    };
    surface.show();
    motion.clock()->setActive(true);
    motion.clock()->advance(0);
    panels.setReducedMotion(true);
    motion.setReducedMotion(true);
    QJsonArray checks;
    QJsonObject context;
    bool passed = true;
    const auto check = [&](const QString &name, bool okay) {
        auto entry = context;
        entry.insert("check", name);
        entry.insert("passed", okay);
        checks.append(entry);
        if (!okay)
            qWarning() << "Library audit failed:" << name;
        passed = okay && passed;
    };
    const QList<QSize> sizes{ { 1920, 1080 }, { 1280, 720 },  { 995, 858 },
                              { 960, 900 },   { 930, 900 },   { 390, 844 },
                              { 320, 720 },   { 2560, 1080 }, { 3440, 1440 } };
    const QList<double> dprs = OffscreenSurface::Enabled
            ? QList<double>{ 1, 1.5, 2 }
            : QList<double>{ window.devicePixelRatio() };
    for (double dpr : dprs) {
        if (OffscreenSurface::Enabled)
            surface.setDevicePixelRatio(dpr);
        for (auto size : sizes) {
            root->setSize(size);
            window.resize(size);
            scene.layout()->setViewport(size.width(), size.height());
            for (bool night : { false, true }) {
                context = { { "width", size.width() },
                            { "height", size.height() },
                            { "dpr", dpr },
                            { "night", night } };
                scene.lighting()->setNight(night);
                panels.open("search");
                check("render-search", render());
                auto *pane = visual(window.contentItem(), "libraryPanelSurface");
                auto *query = visual(window.contentItem(), "librarySearchInput");
                auto *results = visual(window.contentItem(), "librarySearchResults");
                if (!pane || !query || !results)
                    return 4;
                check("source-panel-width",
                      std::abs(pane->width()
                               - std::min(650., size.width() - (size.width() <= 700 ? 24. : 56.)))
                              <= 1);
                auto *sceneItem =
                        qobject_cast<rhi::SceneItem *>(visual(window.contentItem(), "nativeScene"));
                check("scene-error-free", sceneItem && sceneItem->errorText().isEmpty());
                check("panel-descriptor", sceneItem && sceneItem->input().panelLayerSize == size);
                if (dpr == 1 && size == QSize(1280, 720) && !night) {
                    query->setProperty("text", "Song");
                    render();
                    check("large-results", library.searchResults()->rowCount() == 4000);
                    int live = 0;
                    for (auto *item : results->childItems())
                        live += item->childItems().size();
                    check("large-result-virtualization", live < 100);
                    query->setProperty("text", "");
                }
                check("search-focus-and-modal", query->hasActiveFocus() && !input.enabled());
                for (const auto character : QByteArray("Song 199"))
                    QTest::keyClick(&window, character);
                check("continuous-input", library.searchResults()->rowCount() == 20);
                query->setProperty("text", "unmatched");
                check("empty-results", library.searchResults()->rowCount() == 0);
                query->setProperty("text", "");
                check("empty-query-albums", library.searchResults()->rowCount() == 20);
                int delegates = 0;
                for (auto *item : results->childItems())
                    delegates += item->childItems().size();
                check("virtual-search-list", delegates < 100);
                render();
                if ((size == QSize(1280, 720) || size == QSize(390, 844)) && dpr == 1)
                    check("own-image",
                          surface.grab().save(
                                  output
                                  + QString("/search-%1-%2.png").arg(size.width()).arg(night)));
                QPointer<QQuickItem> backdrop = visual(window.contentItem(), "panelBackdropLoader");
                panels.close();
                check("close-search", render() && !panels.surface()->shown() && input.enabled());
                if (dpr == 1 && size == QSize(1280, 720) && !night)
                    qInfo() << "Closed backdrop" << bool(backdrop)
                            << (backdrop ? backdrop->property("active") : QVariant())
                            << (backdrop ? backdrop->childItems().size() : -1)
                            << (sceneItem ? sceneItem->input().panelLayerSize : QSize());
                check("closed-backdrop-released",
                      backdrop && !backdrop->property("active").toBool()
                              && backdrop->childItems().isEmpty() && sceneItem
                              && sceneItem->input().panelLayerSize.isEmpty());
                panels.open("library");
                check("render-library", render());
                if (size == QSize(1280, 720) && dpr == 1)
                    check("own-library-image",
                          surface.grab().save(output + QString("/library-%1.png").arg(night)));
                panels.close();
                render();
            }
        }
    }
    context = { { "scenario", "many-genres" }, { "dpr", dprs.last() } };
    LibrarySnapshot large = snapshot;
    for (int i = 0; i < 1024; ++i) {
        const auto id = QString("genre-%1").arg(i);
        large.genres.append({ id, QString("分类 %1").arg(i), { }, { }, { } });
        Album album;
        album.id = QString("extra-album-%1").arg(i);
        album.title = album.id;
        album.genreId = id;
        large.albums.append(album);
    }
    library.setSnapshot(large);
    selection.setColumns(library.albums()->order().columns, library.snapshot());
    panels.open("search");
    check("large-genres-render", render());
    auto *genres = visual(window.contentItem(), "libraryGenreFilters");
    int liveGenreRows = 0;
    if (genres)
        for (auto *child : genres->childItems())
            liveGenreRows += child->childItems().size();
    check("large-genres-virtualization",
          genres && liveGenreRows < 100 && library.genres().size() == 1026);
    panels.close();
    library.setSnapshot(snapshot);
    selection.setColumns(library.albums()->order().columns, library.snapshot());
    presentation.reset();
    context = {
        { "scenario", "track-reveal" }, { "width", 1280 }, { "height", 720 }, { "dpr", dprs.last() }
    };
    window.resize(1280, 720);
    root->setSize({ 1280, 720 });
    scene.layout()->setViewport(1280, 720);
    render();
    motion.setReducedMotion(false);
    QSignalSpy reveal(&focus, &TrackFocusController::revealRequested);
    QSignalSpy focused(&focus, &TrackFocusController::focusRequested);
    check("request-last-track", focus.request("album-12", "track-199"));
    for (int frame = 0; frame < 2400 && focused.isEmpty(); ++frame) {
        motion.clock()->advance(1. / 60);
        if (frame % 4 == 0)
            render();
    }
    render();
    check("detail-track-scroll-and-focus",
          !focused.isEmpty() && reveal.size() == 1 && library.navigationAlbumId() == "album-12"
                  && library.navigationTrackId() == "track-199");
    qInfo() << "Focus actual"
            << (window.activeFocusItem() ? window.activeFocusItem()->metaObject()->className()
                                         : "null")
            << (window.activeFocusItem() ? window.activeFocusItem()->objectName() : QString())
            << focus.active() << focus.revision() << focus.scrollY();
    check("actual-track-focus",
          window.activeFocusItem()
                  && window.activeFocusItem()->property("trackId").toString() == "track-199");
    auto *trackList = visual(window.contentItem(), "detailTrackList");
    auto *trackView = trackList
            ? qobject_cast<QQuickItem *>(trackList->property("currentView").value<QObject *>())
            : nullptr;
    auto *focusedRow = window.activeFocusItem();
    const double lastBottom = focusedRow && trackView
            ? focusedRow->mapToItem(trackView, { 0, focusedRow->height() }).y()
            : -1;
    qInfo() << "Track geometry" << trackView << lastBottom
            << (trackView ? trackView->property("contentY") : QVariant())
            << (trackView ? trackView->property("contentHeight") : QVariant())
            << (trackView ? trackView->property("originY") : QVariant());
    check("last-row-clamped-to-content",
          trackView && std::abs(lastBottom - trackView->height()) <= 1);
    const auto layout = presentation.layoutRevision();
    if (trackList && trackList->parentItem())
        trackList->parentItem()->setProperty("activeTab", 1);
    check("reuse-open-album", focus.request("album-12", "track-100"));
    render();
    for (int frame = 0; frame < 240
         && (!window.activeFocusItem()
             || window.activeFocusItem()->property("trackId").toString() != "track-100");
         ++frame) {
        motion.clock()->advance(1. / 60);
        if (frame % 4 == 0)
            render();
    }
    render();
    trackView = trackList
            ? qobject_cast<QQuickItem *>(trackList->property("currentView").value<QObject *>())
            : nullptr;
    auto *middleRow = window.activeFocusItem();
    const double center = middleRow && trackView
            ? middleRow
                      ->mapToItem(trackView,
                                  { 0,
                                    middleRow->property("trackTop").toDouble()
                                            + middleRow->property("trackHeight").toDouble() / 2 })
                      .y()
            : -1;
    qInfo() << "Middle focus" << (middleRow ? middleRow->property("trackId") : QVariant()) << center
            << (trackView ? trackView->height() : -1)
            << (trackList ? trackList->property("tabHeight") : QVariant())
            << (middleRow ? middleRow->property("trackTop") : QVariant())
            << (middleRow ? middleRow->property("trackHeight") : QVariant())
            << (trackView ? trackView->property("contentY") : QVariant())
            << (trackView ? trackView->property("originY") : QVariant()) << focus.scrollY();
    check("disc-heading-track-centered",
          middleRow && middleRow->property("trackId").toString() == "track-100" && trackList
                  && trackView
                  && std::abs(center
                              - (trackView->height() + trackList->property("tabHeight").toDouble())
                                      / 2)
                          <= 1);
    check("no-reextract", presentation.layoutRevision() == layout && presentation.detailEnabled());
    auto *tracks = visual(window.contentItem(), "detailTrackList");
    if (tracks)
        QMetaObject::invokeMethod(tracks, "cancelReveal");
    check("user-cancel", !focus.active());
    check("playback-unchanged", playback.isEmpty() && library.playbackTrackId() == "track-0");
    check("qml-warnings", warnings.isEmpty());
    const auto policy = verification::windowPolicy(window);
    check("window-policy", policy.value("passed").toBool());
    QFile executable(QCoreApplication::applicationFilePath());
    QCryptographicHash executableHash(QCryptographicHash::Sha256);
    const bool hashed = executable.open(QIODevice::ReadOnly) && executableHash.addData(&executable);
    QJsonObject report{
        { "passed", passed },
        { "checks", checks },
        { "windowPolicy", policy },
        { "backend", window.property("libraryAuditDevice").toMap().value("backend").toString() },
        { "gpu", window.property("libraryAuditDevice").toMap().value("gpu").toString() },
        { "os", QSysInfo::prettyProductName() },
        { "architecture", QSysInfo::currentCpuArchitecture() },
        { "qt", qVersion() },
        { "binarySha256",
          hashed ? QString::fromLatin1(executableHash.result().toHex()) : QString() },
        { "fixtureSha256",
          QString::fromLatin1(
                  QCryptographicHash::hash(
                          QJsonDocument(toJson(snapshot)).toJson(QJsonDocument::Compact),
                          QCryptographicHash::Sha256)
                          .toHex()) },
        { "offscreen", OffscreenSurface::Enabled },
        { "scope",
          "source geometry, normal output, Qt focus/input, virtual delegates and track reveal; no image scoring or display latency claims" }
    };
    QSaveFile file(output + "/report.json");
    if (!file.open(QIODevice::WriteOnly))
        return 5;
    file.write(QJsonDocument(report).toJson());
    if (!file.commit())
        return 5;
    object.reset();
    surface.hide();
    return passed ? 0 : 6;
}
