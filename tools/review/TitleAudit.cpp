#include "OffscreenSurface.h"
#include "TitleAudit.h"
#include "TitleReel.h"
#include "ReelInk.h"
#include "CanvasTitleProbe.h"
#include "AssetCatalog.h"
#include "ImageComparison.h"
#include "VerificationWindow.h"
#include "LayoutMetrics.h"
#include "LibrarySession.h"
#include <QGuiApplication>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QTimer>
#include <QMouseEvent>
#include <QFontMetricsF>
#include <rhi/qrhi.h>
#include <atomic>
#include <functional>
#include <cmath>
#include <tuple>

namespace {
double trackError(const QJsonArray &samples)
{
    double error = 0;
    int fps = 0;
    yalahoo::ReelTrack track;
    for (const auto &entry : samples) {
        const auto value = entry.toObject();
        if (fps != value["fps"].toInt()) {
            fps = value["fps"].toInt();
            track.move(0, true);
        }
        const auto op = value["op"].toObject();
        if (op.contains("target"))
            track.move(op["target"].toDouble());
        else
            track.advance(op["dt"].toDouble());
        const auto expected = value["sample"].toObject();
        const auto actual = track.sample();
        error = std::max(
                { error, std::abs(actual.value - expected["value"].toArray()[0].toDouble()),
                  std::abs(actual.velocity - expected["velocity"].toArray()[0].toDouble()) });
    }
    return error;
}
QJsonObject structure(yalahoo::TitleReel &controller, const QJsonObject &source)
{
    double error = std::max(std::abs(controller.fittedSize() - source["size"].toDouble()),
                            std::abs(controller.layoutHeight() - source["height"].toDouble()));
    const auto glyphs = source["glyphs"].toArray();
    int checked = 0;
    QString actualText, expectedText;
    QJsonArray actualGlyphs;
    QList<const yalahoo::ReelSlot *> ordered;
    for (int i = 0; i < controller.slotCount(); ++i)
        if (controller.slotAt(i)->active)
            ordered.append(controller.slotAt(i));
    std::sort(ordered.begin(), ordered.end(), [](const auto *a, const auto *b) {
        const auto key = [](const QString &value) {
            return std::tuple(value.startsWith("translation"), value.section(':', 1, 1).toInt(),
                              value.section(':', 2, 2).toInt());
        };
        return key(a->key) < key(b->key);
    });
    for (const auto &entry : glyphs) {
        const auto glyph = entry.toObject();
        expectedText += glyph["text"].toString();
    }
    for (const auto *slot : ordered) {
        const auto &actual = slot->glyph;
        QJsonArray rawFonts;
        for (const auto &run : actual.renderRuns)
            rawFonts.append(QJsonObject{
                    { "family", run.font.family() },
                    { "size", run.font.pixelSize() },
                    { "weight", int(run.font.weight()) },
                    { "ascent", QFontMetricsF(run.font).ascent() },
                    { "descent", QFontMetricsF(run.font).descent() },
                    { "style", run.font.styleName() },
                    { "firstY", run.positions.isEmpty() ? 0 : run.positions.first().y() } });
        actualGlyphs.append(QJsonObject{ { "key", actual.key },
                                         { "text", actual.text },
                                         { "x", actual.x },
                                         { "y", actual.y },
                                         { "width", actual.width },
                                         { "height", actual.height },
                                         { "baseline", actual.baseline },
                                         { "scale", actual.scale },
                                         { "colorGlyph", actual.colorGlyph },
                                         { "fonts", rawFonts } });
        actualText += actual.text;
        for (const auto &entry : glyphs) {
            const auto expected = entry.toObject();
            if (expected["key"].toString() == actual.key
                && expected["text"].toString() == actual.text) {
                error = std::max({ error, std::abs(actual.x - expected["x"].toDouble()),
                                   std::abs(actual.y - expected["y"].toDouble()),
                                   std::abs(actual.width - expected["width"].toDouble()),
                                   std::abs(actual.height - expected["height"].toDouble()) });
                ++checked;
                break;
            }
        }
    }
    return { { "passed", error <= 1 && actualText == expectedText },
             { "actualGlyphs", actualGlyphs },
             { "maximumError", error },
             { "matchingUnits", checked },
             { "actualText", actualText },
             { "expectedText", expectedText } };
}
double rollingError(const QJsonArray &samples)
{
    yalahoo::ReelSlot slot;
    slot.transport = true;
    slot.glyph.text = "A";
    slot.glyph.height = 29;
    slot.glyph.width = 9;
    slot.glyph.size = 12;
    slot.glyph.font = yalahoo::assetFont(400);
    slot.viewport.move(29, true);
    slot.roll.sampled = true;
    slot.rollTo(slot.glyph, false);
    slot.active = true;
    yalahoo::ReelTrack roll;
    roll.sampled = true;
    roll.move(2, false, { .12, 12 });
    double error = 0, elapsed = 0;
    for (const auto &entry : samples) {
        const auto sample = entry.toObject();
        const double dt = sample["dt"].toDouble() - elapsed;
        elapsed += dt;
        slot.advance(dt);
        roll.advance(dt);
        const auto expectedEntry = sample["entry"].toObject();
        const auto expectedRoll = sample["roll"].toObject();
        error = std::max(
                { error,
                  std::abs(slot.entry.sample().value / 29 - expectedEntry["position"].toDouble()),
                  std::abs(slot.entry.sample().velocity / 29
                           - expectedEntry["velocity"].toDouble()),
                  std::abs(slot.blurOpacity()
                           - sample["entryBlur"].toObject()["position"].toDouble()),
                  std::abs(roll.sample().value - expectedRoll["position"].toDouble()),
                  std::abs(roll.sample().velocity - expectedRoll["velocity"].toDouble()) });
    }
    return error;
}
}
int reviewTitle(QGuiApplication &app, const QString &output, const QString &source)
{
    const QString sourceDirectory = source.isEmpty()
            ? QStringLiteral(FIXTURE_ROOT "/verification/windows/title-reels/web")
            : QDir(source).absolutePath();
    QFile input(sourceDirectory + "/report.json");
    if (!input.open(QIODevice::ReadOnly))
        return 3;
    const auto reference = QJsonDocument::fromJson(input.readAll()).object();
    if (reference["captures"].toArray().isEmpty() || reference["tracks"].toArray().isEmpty()
        || reference["rolling"].toArray().isEmpty())
        return 3;
    QDir().mkpath(output);
    QQmlEngine engine;
    yalahoo::LayoutMetrics metrics;
    yalahoo::FrameClock clock;
    clock.setActive(true);
    clock.advance(0);
    yalahoo::TitleReel controller;
    controller.setClock(&clock);
    controller.setFont(yalahoo::assetFont(700));
    controller.setSubtitleFont(yalahoo::assetFont(600));
    controller.setReducedMotion(true);
    controller.componentComplete();
    QQmlComponent component(&engine, QUrl("qrc:/qt/qml/Yalahoo/Music/TitleReview.qml"));
    std::unique_ptr<QObject> object(component.createWithInitialProperties(
            { { "controller", QVariant::fromValue(&controller) },
              { "metrics", QVariant::fromValue(&metrics) },
              { "clock", QVariant::fromValue(&clock) } }));
    auto *root = qobject_cast<QQuickItem *>(object.get());
    if (!root) {
        qWarning() << component.errors();
        return 3;
    }
    yalahoo::LibrarySession session(nullptr);
    yalahoo::LibrarySnapshot library;
    for (int i = 0; i < 3; ++i) {
        yalahoo::Album album;
        album.id = QString("album-%1").arg(i);
        album.title = i % 2 ? QStringLiteral("新的旅程（Music stories）")
                            : QStringLiteral("收藏的声音（Archive gyp）");
        yalahoo::Track track;
        track.id = QString("track-%1").arg(i);
        track.albumId = album.id;
        track.title = i == 0 ? QStringLiteral("初始歌曲 gyp é")
                : i == 1
                ? QStringLiteral("新的歌曲（gyp）👨‍👩‍👧‍👦 é")
                : QStringLiteral("最终歌曲 gyp（收藏）👨‍👩‍👧‍👦 é");
        album.tracks.append(track);
        library.albums.append(album);
    }
    session.setSnapshot(library);
    int playbackChanges = 0;
    QObject::connect(&session, &yalahoo::LibrarySession::playbackChanged, &app, [&] {
        ++playbackChanges;
        const auto *track =
                session.snapshot()->track(session.playbackAlbumId(), session.playbackTrackId());
        root->setProperty("song", track ? track->title : QString());
    });
    QObject::connect(&session, &yalahoo::LibrarySession::browseInfoChanged, &app, [&] {
        controller.setText(session.browseInfo()["title"].toString());
        controller.reconcile();
    });
    QQuickRenderControl renderControl;
    QQuickWindow window(OffscreenSurface::Enabled ? &renderControl : nullptr);
    OffscreenSurface surface(window, renderControl);
    root->setProperty("offscreenRendering", OffscreenSurface::Enabled);
    yalahoo::verification::prepareWindow(window, true);
    window.setPersistentSceneGraph(false);
    window.setPersistentGraphics(false);
    root->setParentItem(window.contentItem());
    window.resize(302, 152);
    root->setSize(window.size());
    controller.setDpr(window.devicePixelRatio());
    QJsonArray cases;
    for (const auto &entry : reference["captures"].toArray())
        if (std::abs(entry.toObject()["dpr"].toDouble() - window.devicePixelRatio()) < .001)
            cases.append(entry);
    if (cases.isEmpty())
        return 3;
    const double motionError = std::max(trackError(reference["tracks"].toArray()),
                                        rollingError(reference["rolling"].toArray()));
    bool passed = motionError <= 1e-6;
    QJsonArray captures, lifecycle, costs, animatedCosts, headerChecks;
    QElapsedTimer timer;
    timer.start();
    std::atomic<qint64> syncStart{ 0 }, renderStart{ 0 }, syncNs{ 0 }, renderNs{ 0 };
    QObject::connect(
            &window, &QQuickWindow::beforeSynchronizing, &window,
            [&] {
                syncStart = timer.nsecsElapsed();
            },
            Qt::DirectConnection);
    QObject::connect(
            &window, &QQuickWindow::afterSynchronizing, &window,
            [&] {
                syncNs += timer.nsecsElapsed() - syncStart;
            },
            Qt::DirectConnection);
    QObject::connect(
            &window, &QQuickWindow::beforeRendering, &window,
            [&] {
                renderStart = timer.nsecsElapsed();
            },
            Qt::DirectConnection);
    QObject::connect(
            &window, &QQuickWindow::afterRendering, &window,
            [&] {
                renderNs += timer.nsecsElapsed() - renderStart;
            },
            Qt::DirectConnection);
    std::atomic<bool> requested{ false }, inFlight{ false }, restorePending{ false };
    std::atomic<int> drain{ 0 };
    QRhiReadbackResult readback;
    int step = 0, warmFrames = 0, marker = 0;
    int animatedFrame = 0;
    bool animationRun = false, headerRun = false, noFrameShaping = true;
    qint64 tickNs = 0;
    QJsonArray frameRecords;
    QPointF initialPlayPosition, initialStopPosition;
    bool fixedButtons = true;
    QJsonArray headerPositions;
    quint64 lifecycleNodeLimit = 0;
    int lifecycleRendererLimit = 0;
    const QList<QSize> productSizes{ { 1920, 1080 }, { 1280, 720 },  { 995, 858 },
                                     { 960, 900 },   { 930, 900 },   { 390, 844 },
                                     { 320, 720 },   { 2560, 1080 }, { 3440, 1440 } };
    qint64 guiNs = 0, beginSync = 0, beginRender = 0;
    qint64 beginCanvasSync = 0, beginCanvasPaint = 0;
    quint64 beginCanvasPaints = 0;
    quint64 beginGeometry = 0, beginReuse = 0;
    std::function<void()> next;
    std::function<void(QImage)> completed;
    QObject::connect(
            &window, &QQuickWindow::sceneGraphInvalidated, &window,
            [&] {
                QMetaObject::invokeMethod(
                        &window,
                        [&] {
                            if (!restorePending.exchange(false))
                                return;
                            surface.show();
                            root->setProperty("marker", ++marker);
                            surface.requestFrame();
                        },
                        Qt::QueuedConnection);
            },
            Qt::DirectConnection);
    QObject::connect(
            &window, &QQuickWindow::afterRendering, &window,
            [&] {
                if (!requested || inFlight)
                    return;
                auto *rhi = surface.rhi();
                auto *commandBuffer = surface.commandBuffer();
                if (!rhi || !commandBuffer)
                    return;
                requested = false;
                inFlight = true;
                drain = rhi->resourceLimit(QRhi::MaxAsyncReadbackFrames) + 1;
                const bool flip = rhi->isYUpInFramebuffer();
                readback.completed = [&, flip] {
                    const auto format = readback.format == QRhiTexture::BGRA8
                            ? QImage::Format_ARGB32
                            : QImage::Format_RGBA8888;
                    QImage frame(reinterpret_cast<const uchar *>(readback.data.constData()),
                                 readback.pixelSize.width(), readback.pixelSize.height(), format);
                    QImage copy = flip ? frame.flipped(Qt::Vertical) : frame.copy();
                    inFlight = false;
                    drain = 0;
                    QMetaObject::invokeMethod(
                            &window,
                            [&, copy] {
                                completed(copy);
                            },
                            Qt::QueuedConnection);
                };
                auto *batch = rhi->nextResourceUpdateBatch();
                batch->readBackTexture(surface.readbackDescription(), &readback);
                commandBuffer->resourceUpdate(batch);
            },
            Qt::DirectConnection);
    surface.connectFrames(&app, [&] {
        if (warmFrames > 0) {
            if (animationRun || headerRun) {
                if (animationRun && animatedFrame % 40 == 0) {
                    session.navigate(animatedFrame % 80 == 0 ? "album-0" : "album-1");
                }
                if (headerRun) {
                    auto *play = root->findChild<QQuickItem *>("playButton");
                    auto *stop = root->findChild<QQuickItem *>("stopButton");
                    if (play && stop) {
                        const auto playPosition = play->mapToScene({ 0, 0 });
                        const auto stopPosition = stop->mapToScene({ 0, 0 });
                        // Establish the prepared layout before the first playback change at 8.
                        if (animatedFrame == 7) {
                            initialPlayPosition = playPosition;
                            initialStopPosition = stopPosition;
                        }
                        if (animatedFrame >= 7)
                            fixedButtons &= QLineF(initialPlayPosition, playPosition).length() < .01
                                    && QLineF(initialStopPosition, stopPosition).length() < .01;
                        headerPositions.append(QJsonObject{ { "frame", animatedFrame },
                                                            { "playX", playPosition.x() },
                                                            { "playY", playPosition.y() },
                                                            { "stopX", stopPosition.x() },
                                                            { "stopY", stopPosition.y() } });
                    } else {
                        fixedButtons = false;
                    }
                }
                const auto before = controller.diagnostics()["layoutMisses"];
                if (headerRun && animatedFrame == 7)
                    root->setProperty("testReduced", false);
                if (headerRun && animatedFrame == 8)
                    session.setPlaybackIds("album-1", "track-1");
                if (headerRun && animatedFrame == 15)
                    clock.setSpeed(.25);
                if (headerRun && animatedFrame == 21) {
                    clock.setSpeed(3);
                    session.setPlaybackIds("album-2", "track-2");
                }
                if (headerRun && animatedFrame == 25)
                    root->setProperty("testReduced", true);
                QElapsedTimer tickTimer;
                tickTimer.start();
                clock.advance(animationRun ? 1.0 / 120 : 1.0 / 60);
                const auto tick = tickTimer.nsecsElapsed();
                tickNs += tick;
                if (animationRun)
                    frameRecords.append(QJsonObject{
                            { "frame", animatedFrame },
                            { "stateForNextSubmissionTickCpuNs", double(tick) },
                            { "synchronizationCpuCumulativeNs", double(syncNs.load()) },
                            { "renderSubmitCpuCumulativeNs", double(renderNs.load()) },
                            { OffscreenSurface::Enabled ? "offscreenFrameCompletedNs"
                                                        : "presentEventNs",
                              double(timer.nsecsElapsed()) } });
                noFrameShaping &= controller.diagnostics()["layoutMisses"] == before;
                ++animatedFrame;
            }
            --warmFrames;
            requested = warmFrames == 0;
            root->setProperty("marker", ++marker);
            surface.requestFrame();
        } else if (inFlight && drain > 0) {
            --drain;
            root->setProperty("marker", ++marker);
            surface.requestFrame();
        }
    });
    const int staticSteps = cases.size() * 3;
    const int animationSteps = 9;
    const int headerSteps = productSizes.size() * 2;
    completed = [&](QImage image) {
        const int index = step - 1;
        if (index < staticSteps) {
            const auto test = cases[index / 3].toObject();
            const int mode = index % 3;
            const QImage expected(sourceDirectory + '/' + test["file"].toString());
            const QImage alpha(sourceDirectory + '/' + test["outlineFile"].toString());
            const auto actual = image.copy(0, 0, expected.width(), expected.height());
            const QString filename = QString("native-%1-mode-%2.png").arg(index / 3).arg(mode);
            const bool valid = !actual.isNull() && actual.save(output + '/' + filename)
                    && yalahoo::verification::windowPolicySatisfied(window);
            const QRectF logicalRoi(0, 0, test["width"].toDouble() * window.devicePixelRatio(),
                                    controller.layoutHeight() * window.devicePixelRatio());
            const QPolygonF roi(logicalRoi);
            const auto raw = yalahoo::compareImages(actual, expected, roi);
            const auto contour = alpha.copy(0, 0, int(std::round(logicalRoi.width() * 8)),
                                            int(std::round(logicalRoi.height() * 8)));
            const auto band = yalahoo::compareAntialiasBand(actual, expected, contour, roi, 255, 8);
            const auto geometry = structure(controller, test["state"].toObject());
            const bool candidatePassed = valid && geometry["passed"].toBool()
                    && band["valid"].toBool() && band["ssim"].toDouble() >= .98
                    && band["meanAbsoluteRgb"].toDouble() <= 6;
            if (mode == 0)
                passed &= candidatePassed;
            captures.append(QJsonObject{
                    { "case", index / 3 },
                    { "mode", mode },
                    { "file", filename },
                    { "valid", valid },
                    { "candidatePassed", candidatePassed },
                    { "raw", raw },
                    { "antialiasBand", band },
                    { "geometry", geometry },
                    { "resources", QJsonObject::fromVariantMap(controller.diagnostics()) } });
            QSaveFile progress(output + "/captures.json");
            if (progress.open(QIODevice::WriteOnly)) {
                progress.write(QJsonDocument(captures).toJson());
                progress.commit();
            }
            costs.append(
                    QJsonObject{ { "mode", mode },
                                 { "guiStateNs", guiNs },
                                 { "syncCpuNs", double(syncNs.load() - beginSync) },
                                 { "renderSubmitCpuNs", double(renderNs.load() - beginRender) },
                                 { "gpuTime", QJsonValue::Null } });
        } else if (index < staticSteps + animationSteps) {
            const int mode = (index - staticSteps) % 3;
            const bool nativeMode = mode == 0;
            const auto reused = yalahoo::ReelInk::geometryReuses() - beginReuse;
            const auto canvasPaints = canvasTitleStats().paints.load() - beginCanvasPaints;
            const bool valid = !image.isNull() && noFrameShaping && (!nativeMode || reused > 0)
                    && (nativeMode || yalahoo::ReelInk::geometryUpdates() == beginGeometry)
                    && (mode == 2 || canvasPaints == 0)
                    && yalahoo::verification::windowPolicySatisfied(window);
            passed &= valid;
            animatedCosts.append(QJsonObject{
                    { "run", (index - staticSteps) / 3 },
                    { "mode", (index - staticSteps) % 3 },
                    { "passed", valid },
                    { "frames", animatedFrame },
                    { "guiStateNs", double(guiNs) },
                    { "clockTickCpuNs", double(tickNs) },
                    { "samples", frameRecords },
                    { "syncCpuNs", double(syncNs.load() - beginSync) },
                    { "renderSubmitCpuNs", double(renderNs.load() - beginRender) },
                    { "gpuTime", QJsonValue::Null },
                    { "canvasSyncCpuNs",
                      double(canvasTitleStats().syncNs.load() - beginCanvasSync) },
                    { "canvasPaintCpuNs",
                      double(canvasTitleStats().paintNs.load() - beginCanvasPaint) },
                    { "canvasPaints", double(canvasPaints) },
                    { "glyphNodesCreated",
                      double(yalahoo::ReelInk::geometryUpdates() - beginGeometry) },
                    { "glyphNodesReused", double(reused) },
                    { "resources", QJsonObject::fromVariantMap(controller.diagnostics()) } });
        } else if (index < staticSteps + animationSteps + headerSteps) {
            auto *song = root->findChild<QQuickItem *>("playingTitle");
            auto *play = root->findChild<QQuickItem *>("playButton");
            auto *stop = root->findChild<QQuickItem *>("stopButton");
            bool valid = song && play && stop && !image.isNull() && fixedButtons
                    && yalahoo::verification::windowPolicySatisfied(window);
            const QString full = root->property("song").toString();
            const int beforePlaybackChanges = playbackChanges;
            const QString beforeAlbum = session.playbackAlbumId();
            const QString beforeTrack = session.playbackTrackId();
            QVariant namesResult;
            const bool fullNames = song
                    && QMetaObject::invokeMethod(root, "fullTitleNames",
                                                 Q_RETURN_ARG(QVariant, namesResult),
                                                 Q_ARG(QVariant, QVariant::fromValue(song)))
                    && namesResult.toBool();
            valid &= fullNames;
            if (valid && song->width() > 0) {
                const QPointF point = song->mapToScene({ song->width() / 2, song->height() / 2 });
                QMouseEvent press(QEvent::MouseButtonPress, point,
                                  window.mapToGlobal(point.toPoint()), Qt::LeftButton,
                                  Qt::LeftButton, Qt::NoModifier);
                QMouseEvent release(QEvent::MouseButtonRelease, point,
                                    window.mapToGlobal(point.toPoint()), Qt::LeftButton,
                                    Qt::NoButton, Qt::NoModifier);
                QCoreApplication::sendEvent(&window, &press);
                QCoreApplication::sendEvent(&window, &release);
                valid &= root->property("lastAction").toString() == "locate-song";
                session.navigate(beforeAlbum, beforeTrack);
            }
            const bool playbackUnchanged = beforePlaybackChanges == playbackChanges
                    && beforeAlbum == session.playbackAlbumId()
                    && beforeTrack == session.playbackTrackId();
            valid &= playbackUnchanged;
            auto *header = root->findChild<QQuickItem *>("archiveHeader");
            auto *transportReel = header
                    ? qvariant_cast<yalahoo::TitleReel *>(header->property("transportController"))
                    : nullptr;
            const bool reducedFinished = transportReel && !transportReel->moving();
            valid &= reducedFinished;
            const double songWidth = song ? song->width() : 0;
            root->setProperty("testReduced", false);
            root->setProperty("song", "");
            const bool exitDisabled = song && !song->isEnabled() && song->width() == songWidth
                    && song->property("retainedTitle").toString() == full;
            valid &= exitDisabled;
            const auto playPosition = play ? play->mapToScene({ 0, 0 }) : QPointF();
            const auto stopPosition = stop ? stop->mapToScene({ 0, 0 }) : QPointF();
            if (play && stop)
                valid &= playPosition.x() + play->width() <= window.width()
                        && stopPosition.x() + stop->width() <= window.width();
            else
                valid = false;
            const QString filename =
                    QString("header-%1.png").arg(index - staticSteps - animationSteps);
            valid &= image.save(output + '/' + filename);
            passed &= valid;
            headerChecks.append(QJsonObject{ { "passed", valid },
                                             { "file", filename },
                                             { "title", full },
                                             { "action", root->property("lastAction").toString() },
                                             { "fixedButtons", fixedButtons },
                                             { "positions", headerPositions },
                                             { "fullNames", fullNames },
                                             { "playbackUnchanged", playbackUnchanged },
                                             { "reducedFinished", reducedFinished },
                                             { "exitDisabled", exitDisabled },
                                             { "width", window.width() },
                                             { "height", window.height() },
                                             { "songWidth", songWidth } });
        } else {
            const auto nodes = yalahoo::ReelInk::liveNodes();
            const auto renderers = canvasTitleStats().renderers.load();
            // Each mode has a different owner; the bounded ceilings cover all ten restores.
            lifecycleNodeLimit = std::max(lifecycleNodeLimit, quint64(controller.slotCount()));
            lifecycleRendererLimit = 1;
            const bool valid = nodes <= int(lifecycleNodeLimit)
                    && renderers <= lifecycleRendererLimit
                    && controller.diagnostics()["cacheBytes"].toInt()
                            <= yalahoo::TextLayoutAdapter::CacheBytes
                    && !image.isNull() && yalahoo::verification::windowPolicySatisfied(window);
            passed &= valid;
            lifecycle.append(QJsonObject{
                    { "cycle", index - staticSteps - animationSteps - headerSteps },
                    { "passed", valid },
                    { "liveInkNodes", nodes },
                    { "liveCanvasRenderers", renderers },
                    { "resources", QJsonObject::fromVariantMap(controller.diagnostics()) } });
        }
        QTimer::singleShot(0, &app, [&] {
            next();
        });
    };
    next = [&] {
        if (step >= staticSteps + animationSteps + headerSteps + 10) {
            root->setParentItem(nullptr);
            object.reset();
            surface.hide();
            window.releaseResources();
            QTimer::singleShot(200, &app, [&] {
                const bool released =
                        yalahoo::ReelInk::liveNodes() == 0 && canvasTitleStats().renderers == 0;
                passed &= released;
                QSaveFile file(output + "/report.json");
                if (!file.open(QIODevice::WriteOnly)) {
                    app.exit(5);
                    return;
                }
                file.write(QJsonDocument(
                                   QJsonObject{
                                           { "passed", passed },
                                           { "motionError", motionError },
                                           { "dpr", window.devicePixelRatio() },
                                           { "qtVersion", qVersion() },
                                           { "offscreen", OffscreenSurface::Enabled },
                                           { "renderProtocol",
                                             OffscreenSurface::Enabled ? "render-control-gui-thread"
                                                                       : "native-window" },
                                           { "offscreenResourcePlan", surface.diagnostics() },
                                           { "backend",
                                             window.rendererInterface()->graphicsApi()
                                                             == QSGRendererInterface::Direct3D11
                                                     ? "d3d11"
                                                     : window.rendererInterface()->graphicsApi()
                                                             == QSGRendererInterface::OpenGL
                                                     ? "opengl"
                                                     : window.rendererInterface()->graphicsApi()
                                                             == QSGRendererInterface::Metal
                                                     ? "metal"
                                                     : "unsupported" },
                                           { "sourceDirectory", sourceDirectory },
                                           { "captures", captures },
                                           { "lifecycle", lifecycle },
                                           { "animatedCosts", animatedCosts },
                                           { "headerChecks", headerChecks },
                                           { "costs", costs },
                                           { "released", released },
                                           { "canvasSyncCpuNs",
                                             double(canvasTitleStats().syncNs.load()) },
                                           { "canvasPaintCpuNs",
                                             double(canvasTitleStats().paintNs.load()) },
                                           { "canvasPaints",
                                             double(canvasTitleStats().paints.load()) },
                                           { "windowPolicy",
                                             yalahoo::verification::windowPolicy(window) } })
                                   .toJson());
                app.exit(file.commit() && passed ? 0 : 6);
            });
            return;
        }
        beginSync = syncNs;
        beginRender = renderNs;
        beginCanvasSync = canvasTitleStats().syncNs;
        beginCanvasPaint = canvasTitleStats().paintNs;
        beginCanvasPaints = canvasTitleStats().paints;
        beginGeometry = yalahoo::ReelInk::geometryUpdates();
        beginReuse = yalahoo::ReelInk::geometryReuses();
        QElapsedTimer guiTimer;
        guiTimer.start();
        if (step < staticSteps) {
            const auto test = cases[step / 3].toObject();
            root->setProperty("mode", step % 3);
            root->setProperty("night", test["night"].toBool());
            const QSize size(test["width"].toInt() + 2, test["budget"].toInt() + 2);
            window.resize(size);
            root->setSize(size);
            controller.setAvailableWidth(test["width"].toDouble());
            controller.setBudget(test["budget"].toDouble());
            controller.setPixelSize(test["size"].toDouble());
            controller.setText(test["text"].toString());
            controller.reconcile();
        } else if (step < staticSteps + animationSteps) {
            animationRun = true;
            headerRun = false;
            animatedFrame = 0;
            tickNs = 0;
            fixedButtons = true;
            noFrameShaping = true;
            frameRecords = { };
            root->setProperty("mode", (step - staticSteps) % 3);
            root->setProperty("night", false);
            controller.setReducedMotion(false);
            controller.setAvailableWidth(300);
            controller.setBudget(150);
            controller.setPixelSize(28);
            window.resize(302, 152);
            root->setSize(window.size());
            controller.reconcile();
        } else if (step < staticSteps + animationSteps + headerSteps) {
            animationRun = false;
            headerRun = true;
            controller.setActive(false);
            animatedFrame = 0;
            tickNs = 0;
            fixedButtons = true;
            headerPositions = { };
            clock.setSpeed(1);
            const int index = step - staticSteps - animationSteps;
            const auto size = productSizes[index / 2];
            window.resize(size);
            root->setSize(size);
            metrics.setViewport(size.width(), size.height());
            root->setProperty("headerMode", true);
            root->setProperty("testReduced", true);
            root->setProperty("night", index % 2 != 0);
            root->setProperty("lastAction", "");
            session.setPlaybackIds("album-0", "track-0");
            root->setProperty("song", library.albums[0].tracks[0].title);
            root->setProperty("playing", index % 2 != 0);
        } else {
            animationRun = false;
            headerRun = false;
            controller.setActive(true);
            root->setProperty("headerMode", false);
            root->setProperty("testReduced", true);
            controller.setReducedMotion(true);
            restorePending = true;
            surface.hide();
            window.releaseResources();
            controller.setAvailableWidth(300 + (step % 2) * 20);
            window.resize(int(controller.availableWidth()) + 2, 152);
            root->setSize(window.size());
            controller.reconcile();
            root->setProperty("mode", (step - staticSteps) % 3);
        }
        guiNs = guiTimer.nsecsElapsed();
        ++step;
        warmFrames = animationRun ? 120 : headerRun ? 60 : 3;
        root->setProperty("marker", ++marker);
        surface.requestFrame();
    };
    surface.show();
    QTimer::singleShot(0, &app, [&] {
        next();
    });
    const double dpr = window.devicePixelRatio();
    // Reference-contour and SSIM comparisons run on CPU over physical pixels.
    QTimer::singleShot(int(90000 * std::max(1.0, dpr * dpr)), &app, [&] {
        QSaveFile file(output + "/timeout.json");
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(QJsonObject{ { "passed", false },
                                                  { "step", step },
                                                  { "staticSteps", staticSteps },
                                                  { "animationSteps", animationSteps },
                                                  { "headerSteps", headerSteps },
                                                  { "warmFrames", warmFrames },
                                                  { "requested", requested.load() },
                                                  { "inFlight", inFlight.load() },
                                                  { "drain", drain.load() },
                                                  { "visible", window.isVisible() },
                                                  { "exposed", window.isExposed() },
                                                  { "restorePending", restorePending.load() },
                                                  { "dpr", dpr },
                                                  { "windowPolicy",
                                                    yalahoo::verification::windowPolicy(window) } })
                               .toJson());
            file.commit();
        }
        app.exit(7);
    });
    const int result = app.exec();
    QObject::disconnect(&window, nullptr, &window, nullptr);
    surface.hide();
    window.releaseResources();
    // Destroy the graphics surface while the pending readback and callback state are alive.
    window.destroy();
    readback.completed = { };
    return result;
}
