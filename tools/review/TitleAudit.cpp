#include "OffscreenSurface.h"
#include "TitleAudit.h"
#include "TitleReel.h"
#include "ReelInk.h"
#include "CanvasTitleProbe.h"
#include "AssetCatalog.h"
#include "VerificationWindow.h"
#include "LayoutMetrics.h"
#include "LibrarySession.h"
#include <QGuiApplication>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QTimer>
#include <QMouseEvent>
#include <QFontMetricsF>
#include <QCryptographicHash>
#include <rhi/qrhi.h>
#include <atomic>
#include <functional>
#include <cmath>
#include <stdexcept>
#include <tuple>
#include <QMutex>
#include <QMutexLocker>

using namespace Qt::StringLiterals;

namespace {
double trackError(const QJsonArray &samples)
{
    double error = 0;
    int fps = 0;
    yalahoo::ReelTrack track;
    for (const auto &entry : samples) {
        const auto value = entry.toObject();
        if (fps != value[u"fps"_s].toInt()) {
            fps = value[u"fps"_s].toInt();
            track.move(0, true);
        }
        const auto op = value[u"op"_s].toObject();
        if (op.contains(u"target"_s))
            track.move(op[u"target"_s].toDouble());
        else
            track.advance(op[u"dt"_s].toDouble());
        const auto expected = value[u"sample"_s].toObject();
        const auto actual = track.sample();
        error = std::max(
                { error, std::abs(actual.value - expected[u"value"_s].toArray()[0].toDouble()),
                  std::abs(actual.velocity - expected[u"velocity"_s].toArray()[0].toDouble()) });
    }
    return error;
}
QJsonObject structure(yalahoo::TitleReel &controller, const QJsonObject &source)
{
    double error = std::max(std::abs(controller.fittedSize() - source[u"size"_s].toDouble()),
                            std::abs(controller.layoutHeight() - source[u"height"_s].toDouble()));
    const auto glyphs = source[u"glyphs"_s].toArray();
    int checked = 0;
    QString actualText, expectedText;
    QJsonArray actualGlyphs;
    QList<const yalahoo::ReelSlot *> ordered;
    for (int i = 0; i < controller.slotCount(); ++i)
        if (controller.slotAt(i)->active)
            ordered.append(controller.slotAt(i));
    std::sort(ordered.begin(), ordered.end(), [](const auto *a, const auto *b) {
        const auto key = [](const QString &value) {
            return std::tuple(value.startsWith(u"translation"_s),
                              value.section(':'_L1, 1, 1).toInt(),
                              value.section(':'_L1, 2, 2).toInt());
        };
        return key(a->key) < key(b->key);
    });
    for (const auto &entry : glyphs) {
        const auto glyph = entry.toObject();
        expectedText += glyph[u"text"_s].toString();
    }
    for (const auto *slot : ordered) {
        const auto &actual = slot->glyph;
        QJsonArray rawFonts;
        for (const auto &run : actual.renderRuns) {
            QJsonArray indexes, positions;
            for (const auto index : run.indexes)
                indexes.append(int(index));
            for (const auto &position : run.positions)
                positions.append(QJsonArray{ position.x(), position.y() });
            rawFonts.append(QJsonObject{
                    { u"family"_s, run.font.family() },
                    { u"size"_s, run.font.pixelSize() },
                    { u"weight"_s, int(run.font.weight()) },
                    { u"ascent"_s, QFontMetricsF(run.font).ascent() },
                    { u"descent"_s, QFontMetricsF(run.font).descent() },
                    { u"style"_s, run.font.styleName() },
                    { u"headSha256"_s,
                      QString::fromUtf8(
                              QCryptographicHash::hash(run.fontHead, QCryptographicHash::Sha256)
                                      .toHex()) },
                    { u"glyphIndexes"_s, indexes },
                    { u"glyphPositions"_s, positions },
                    { u"firstY"_s, run.positions.isEmpty() ? 0 : run.positions.first().y() } });
        }
        actualGlyphs.append(QJsonObject{ { u"key"_s, actual.key },
                                         { u"text"_s, actual.text },
                                         { u"x"_s, actual.x },
                                         { u"y"_s, actual.y },
                                         { u"width"_s, actual.width },
                                         { u"height"_s, actual.height },
                                         { u"baseline"_s, actual.baseline },
                                         { u"scale"_s, actual.scale },
                                         { u"colorGlyph"_s, actual.colorGlyph },
                                         { u"fonts"_s, rawFonts } });
        actualText += actual.text;
        // Compare every visible unit in source order. Internal slot keys can
        // contain gaps after clamping (notably the final ellipsis).
        const int index = actualGlyphs.size() - 1;
        if (index < glyphs.size()) {
            const auto expected = glyphs[index].toObject();
            if (expected[u"text"_s].toString() == actual.text) {
                error = std::max({ error, std::abs(actual.x - expected[u"x"_s].toDouble()),
                                   std::abs(actual.y - expected[u"y"_s].toDouble()),
                                   std::abs(actual.width - expected[u"width"_s].toDouble()),
                                   std::abs(actual.height - expected[u"height"_s].toDouble()) });
                ++checked;
            }
        }
    }
    return { { u"passed"_s,
               error <= 1 && actualText == expectedText && checked == glyphs.size()
                       && actualGlyphs.size() == glyphs.size() },
             { u"actualGlyphs"_s, actualGlyphs },
             { u"maximumError"_s, error },
             { u"matchingUnits"_s, checked },
             { u"actualText"_s, actualText },
             { u"expectedText"_s, expectedText } };
}
QJsonObject layerStructure(QQuickItem *root, const yalahoo::TitleReel &controller, int mode)
{
    auto *reel = root->findChild<QQuickItem *>("reviewTitle");
    QVariant result;
    const bool invoked =
            reel && QMetaObject::invokeMethod(reel, "layerSizes", Q_RETURN_ARG(QVariant, result));
    const auto actual = QJsonArray::fromVariantList(result.toList());
    QJsonArray expected;
    if (mode != 2)
        for (const auto size : controller.resourceSizes())
            expected.append(
                    QJsonObject{ { u"width"_s, size.width() }, { u"height"_s, size.height() } });
    return { { u"passed"_s, invoked && actual == expected },
             { u"actual"_s, actual },
             { u"planned"_s, expected } };
}
double rollingError(const QJsonArray &samples)
{
    yalahoo::ReelSlot slot;
    slot.transport = true;
    slot.glyph.text = "A"_L1;
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
        const double dt = sample[u"dt"_s].toDouble() - elapsed;
        elapsed += dt;
        slot.advance(dt);
        roll.advance(dt);
        const auto expectedEntry = sample[u"entry"_s].toObject();
        const auto expectedRoll = sample[u"roll"_s].toObject();
        error = std::max(
                { error,
                  std::abs(slot.entry.sample().value / 29
                           - expectedEntry[u"position"_s].toDouble()),
                  std::abs(slot.entry.sample().velocity / 29
                           - expectedEntry[u"velocity"_s].toDouble()),
                  std::abs(slot.blurOpacity()
                           - sample[u"entryBlur"_s].toObject()[u"position"_s].toDouble()),
                  std::abs(roll.sample().value - expectedRoll[u"position"_s].toDouble()),
                  std::abs(roll.sample().velocity - expectedRoll[u"velocity"_s].toDouble()) });
    }
    return error;
}
}
int reviewTitle(QGuiApplication &app, const QString &output, const QString &source)
{
    const QString sourceDirectory = source.isEmpty()
            ? (QString::fromUtf8(FIXTURE_ROOT) + u"/verification/windows/title-reels/web"_s)
            : QDir(source).absolutePath();
    QFile input(sourceDirectory + "/report.json"_L1);
    if (!input.open(QIODevice::ReadOnly))
        return 3;
    const auto reference = QJsonDocument::fromJson(input.readAll()).object();
    if (reference[u"protocol"_s].toString() != "source-structure-font-metrics-motion-v1"_L1
        || reference[u"captures"_s].toArray().isEmpty()
        || reference[u"tracks"_s].toArray().isEmpty()
        || reference[u"rolling"_s].toArray().isEmpty())
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
    QQmlComponent component(&engine, QUrl(u"qrc:/qt/qml/Yalahoo/Music/TitleReview.qml"_s));
    std::unique_ptr<QObject> object(component.createWithInitialProperties(
            { { u"controller"_s, QVariant::fromValue(&controller) },
              { u"metrics"_s, QVariant::fromValue(&metrics) },
              { u"clock"_s, QVariant::fromValue(&clock) } }));
    auto *root = qobject_cast<QQuickItem *>(object.get());
    if (!root) {
        qWarning() << component.errors();
        return 3;
    }
    yalahoo::LibrarySession session(nullptr);
    yalahoo::LibrarySnapshot library;
    for (int i = 0; i < 3; ++i) {
        yalahoo::Album album;
        album.id = u"album-%1"_s.arg(i);
        album.title = i % 2 ? u"\u65b0\u7684\u65c5\u7a0b\uff08Music stories\uff09"_s
                            : u"\u6536\u85cf\u7684\u58f0\u97f3\uff08Archive gyp\uff09"_s;
        yalahoo::Track track;
        track.id = u"track-%1"_s.arg(i);
        track.albumId = album.id;
        track.title = i == 0 ? u"\u521d\u59cb\u6b4c\u66f2 gyp e\u0301"_s
                : i == 1
                ? u"\u65b0\u7684\u6b4c\u66f2\uff08gyp\uff09\U0001f468\u200d\U0001f469\u200d\U0001f467\u200d\U0001f466 e\u0301"_s
                : u"\u6700\u7ec8\u6b4c\u66f2 gyp\uff08\u6536\u85cf\uff09\U0001f468\u200d\U0001f469\u200d\U0001f467\u200d\U0001f466 e\u0301"_s;
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
        controller.setText(session.browseInfo()[u"title"_s].toString());
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
    for (const auto &entry : reference[u"captures"_s].toArray())
        if (std::abs(entry.toObject()[u"dpr"_s].toDouble() - window.devicePixelRatio()) < .001)
            cases.append(entry);
    if (cases.isEmpty())
        return 3;
    const double motionError = std::max(trackError(reference[u"tracks"_s].toArray()),
                                        rollingError(reference[u"rolling"_s].toArray()));
    bool passed = motionError <= 1e-6;
    QJsonArray captures, lifecycle, costs, animatedCosts, headerChecks;
    QElapsedTimer timer;
    timer.start();
    std::atomic<qint64> syncStart{ 0 }, renderStart{ 0 }, syncNs{ 0 }, renderNs{ 0 };
    QMutex positionsMutex;
    QPointF synchronizedPlayPosition, synchronizedStopPosition;
    QPointer<QQuickItem> playButton = root->findChild<QQuickItem *>("playButton");
    QPointer<QQuickItem> stopButton = root->findChild<QQuickItem *>("stopButton");
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
                // GUI is blocked here and polish has settled the Row geometry.
                // frameSwapped's GUI callback can observe the next unpolished tick instead.
                if (playButton && stopButton) {
                    const QMutexLocker lock(&positionsMutex);
                    synchronizedPlayPosition = playButton->mapToScene({ 0, 0 });
                    synchronizedStopPosition = stopButton->mapToScene({ 0, 0 });
                }
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
                    QImage copy;
                    try {
                        copy = yalahoo::rhi::readbackImage(readback, flip);
                    } catch (const std::runtime_error &error) {
                        qWarning("Title readback failed: %s", error.what());
                    }
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
                    session.navigate(animatedFrame % 80 == 0 ? u"album-0"_s : u"album-1"_s);
                }
                if (headerRun) {
                    auto *play = root->findChild<QQuickItem *>("playButton");
                    auto *stop = root->findChild<QQuickItem *>("stopButton");
                    if (play && stop) {
                        const QMutexLocker lock(&positionsMutex);
                        const auto playPosition = synchronizedPlayPosition;
                        const auto stopPosition = synchronizedStopPosition;
                        // Establish the prepared layout before the first playback change at 8.
                        if (animatedFrame == 7) {
                            initialPlayPosition = playPosition;
                            initialStopPosition = stopPosition;
                        }
                        if (animatedFrame >= 7)
                            fixedButtons &= QLineF(initialPlayPosition, playPosition).length() < .01
                                    && QLineF(initialStopPosition, stopPosition).length() < .01;
                        headerPositions.append(QJsonObject{ { u"frame"_s, animatedFrame },
                                                            { u"playX"_s, playPosition.x() },
                                                            { u"playY"_s, playPosition.y() },
                                                            { u"stopX"_s, stopPosition.x() },
                                                            { u"stopY"_s, stopPosition.y() } });
                    } else {
                        fixedButtons = false;
                    }
                }
                const auto before = controller.diagnostics()[u"layoutMisses"_s];
                if (headerRun && animatedFrame == 7)
                    root->setProperty("testReduced", false);
                if (headerRun && animatedFrame == 8)
                    session.setPlaybackIds(u"album-1"_s, u"track-1"_s);
                if (headerRun && animatedFrame == 15)
                    clock.setSpeed(.25);
                if (headerRun && animatedFrame == 21) {
                    clock.setSpeed(3);
                    session.setPlaybackIds(u"album-2"_s, u"track-2"_s);
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
                            { u"frame"_s, animatedFrame },
                            { u"stateForNextSubmissionTickCpuNs"_s, double(tick) },
                            { u"synchronizationCpuCumulativeNs"_s, double(syncNs.load()) },
                            { u"renderSubmitCpuCumulativeNs"_s, double(renderNs.load()) },
                            { OffscreenSurface::Enabled ? u"offscreenFrameCompletedNs"_s
                                                        : u"presentEventNs"_s,
                              double(timer.nsecsElapsed()) } });
                noFrameShaping &= controller.diagnostics()[u"layoutMisses"_s] == before;
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
            const QSize captureSize(
                    qRound(test[u"width"_s].toDouble() * window.devicePixelRatio()),
                    qRound(test[u"budget"_s].toDouble() * window.devicePixelRatio()));
            const auto actual = image.copy(QRect(QPoint(), captureSize));
            const QString filename = u"native-%1-mode-%2.png"_s.arg(index / 3).arg(mode);
            const bool valid = !image.isNull() && image.width() >= captureSize.width()
                    && image.height() >= captureSize.height()
                    && actual.save(output + '/'_L1 + filename)
                    && yalahoo::verification::windowPolicySatisfied(window);
            const auto geometry = structure(controller, test[u"state"_s].toObject());
            const auto layers = layerStructure(root, controller, mode);
            const bool structurePassed =
                    valid && geometry[u"passed"_s].toBool() && layers[u"passed"_s].toBool();
            if (mode == 0)
                passed &= structurePassed;
            captures.append(QJsonObject{
                    { u"case"_s, index / 3 },
                    { u"mode"_s, mode },
                    { u"file"_s, filename },
                    { u"valid"_s, valid },
                    { u"structurePassed"_s, structurePassed },
                    { u"geometry"_s, geometry },
                    { u"layers"_s, layers },
                    { u"resources"_s, QJsonObject::fromVariantMap(controller.diagnostics()) } });
            QSaveFile progress(output + "/captures.json"_L1);
            if (progress.open(QIODevice::WriteOnly)) {
                progress.write(QJsonDocument(captures).toJson());
                progress.commit();
            }
            costs.append(
                    QJsonObject{ { u"mode"_s, mode },
                                 { u"guiStateNs"_s, guiNs },
                                 { u"syncCpuNs"_s, double(syncNs.load() - beginSync) },
                                 { u"renderSubmitCpuNs"_s, double(renderNs.load() - beginRender) },
                                 { u"gpuTime"_s, QJsonValue::Null } });
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
                    { u"run"_s, (index - staticSteps) / 3 },
                    { u"mode"_s, (index - staticSteps) % 3 },
                    { u"passed"_s, valid },
                    { u"frames"_s, animatedFrame },
                    { u"guiStateNs"_s, double(guiNs) },
                    { u"clockTickCpuNs"_s, double(tickNs) },
                    { u"samples"_s, frameRecords },
                    { u"syncCpuNs"_s, double(syncNs.load() - beginSync) },
                    { u"renderSubmitCpuNs"_s, double(renderNs.load() - beginRender) },
                    { u"gpuTime"_s, QJsonValue::Null },
                    { u"canvasSyncCpuNs"_s,
                      double(canvasTitleStats().syncNs.load() - beginCanvasSync) },
                    { u"canvasPaintCpuNs"_s,
                      double(canvasTitleStats().paintNs.load() - beginCanvasPaint) },
                    { u"canvasPaints"_s, double(canvasPaints) },
                    { u"glyphNodesCreated"_s,
                      double(yalahoo::ReelInk::geometryUpdates() - beginGeometry) },
                    { u"glyphNodesReused"_s, double(reused) },
                    { u"resources"_s, QJsonObject::fromVariantMap(controller.diagnostics()) } });
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
            const auto *tooltip =
                    song ? qvariant_cast<QObject *>(song->property("toolTip")) : nullptr;
            const bool fullNames = tooltip && tooltip->property("text").toString() == full
                    && QQmlProperty::read(song, u"Accessible.name"_s,
                                          QQmlEngine::contextForObject(song))
                                    .toString()
                            == u"\u5b9a\u4f4d\u6b4c\u66f2\uff1a"_s + full;
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
                valid &= root->property("lastAction").toString() == "locate-song"_L1;
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
            root->setProperty("song", u""_s);
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
            const QString filename = u"header-%1.png"_s.arg(index - staticSteps - animationSteps);
            valid &= image.save(output + '/'_L1 + filename);
            passed &= valid;
            headerChecks.append(
                    QJsonObject{ { u"passed"_s, valid },
                                 { u"file"_s, filename },
                                 { u"title"_s, full },
                                 { u"action"_s, root->property("lastAction").toString() },
                                 { u"fixedButtons"_s, fixedButtons },
                                 { u"positions"_s, headerPositions },
                                 { u"fullNames"_s, fullNames },
                                 { u"playbackUnchanged"_s, playbackUnchanged },
                                 { u"reducedFinished"_s, reducedFinished },
                                 { u"exitDisabled"_s, exitDisabled },
                                 { u"width"_s, window.width() },
                                 { u"height"_s, window.height() },
                                 { u"songWidth"_s, songWidth } });
        } else {
            const auto nodes = yalahoo::ReelInk::liveNodes();
            const auto renderers = canvasTitleStats().renderers.load();
            // Each mode has a different owner; the bounded ceilings cover all ten restores.
            lifecycleNodeLimit = std::max(lifecycleNodeLimit, quint64(controller.slotCount()));
            lifecycleRendererLimit = 1;
            const bool valid = nodes <= int(lifecycleNodeLimit)
                    && renderers <= lifecycleRendererLimit
                    && controller.diagnostics()[u"cacheBytes"_s].toInt()
                            <= yalahoo::TextLayoutAdapter::CacheBytes
                    && !image.isNull() && yalahoo::verification::windowPolicySatisfied(window);
            passed &= valid;
            lifecycle.append(QJsonObject{
                    { u"cycle"_s, index - staticSteps - animationSteps - headerSteps },
                    { u"passed"_s, valid },
                    { u"liveInkNodes"_s, nodes },
                    { u"liveCanvasRenderers"_s, renderers },
                    { u"resources"_s, QJsonObject::fromVariantMap(controller.diagnostics()) } });
        }
        QTimer::singleShot(0, &app, [&] {
            next();
        });
    };
    next = [&] {
        if (step >= staticSteps + animationSteps + headerSteps + 10) {
            root->setParentItem(nullptr);
            object.reset();
            if (!OffscreenSurface::Enabled && qgetenv("QSG_RENDER_LOOP") == "basic"_ba) {
                // Run Qt's queued renderer deletions with a current context before hiding.
                QObject::connect(
                        &window, &QQuickWindow::frameSwapped, &window,
                        [&] {
                            surface.hide();
                            window.releaseResources();
                            window.destroy();
                        },
                        Qt::ConnectionType(Qt::QueuedConnection | Qt::SingleShotConnection));
                surface.requestFrame();
            } else {
                surface.hide();
                window.releaseResources();
            }
            QTimer::singleShot(200, &app, [&] {
                const bool released =
                        yalahoo::ReelInk::liveNodes() == 0 && canvasTitleStats().renderers == 0;
                passed &= released;
                QSaveFile file(output + "/report.json"_L1);
                if (!file.open(QIODevice::WriteOnly)) {
                    app.exit(5);
                    return;
                }
                file.write(
                        QJsonDocument(
                                QJsonObject{ { u"passed"_s, passed },
                                             { u"motionError"_s, motionError },
                                             { u"dpr"_s, window.devicePixelRatio() },
                                             { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
                                             { u"offscreen"_s, OffscreenSurface::Enabled },
                                             { u"renderProtocol"_s,
                                               OffscreenSurface::Enabled
                                                       ? u"render-control-gui-thread"_s
                                                       : u"native-window"_s },
                                             { u"offscreenResourcePlan"_s, surface.diagnostics() },
                                             { u"backend"_s,
                                               window.rendererInterface()->graphicsApi()
                                                               == QSGRendererInterface::Direct3D11
                                                       ? u"d3d11"_s
                                                       : window.rendererInterface()->graphicsApi()
                                                               == QSGRendererInterface::OpenGL
                                                       ? u"opengl"_s
                                                       : window.rendererInterface()->graphicsApi()
                                                               == QSGRendererInterface::Metal
                                                       ? u"metal"_s
                                                       : u"unsupported"_s },
                                             { u"sourceDirectory"_s, sourceDirectory },
                                             { u"captures"_s, captures },
                                             { u"lifecycle"_s, lifecycle },
                                             { u"animatedCosts"_s, animatedCosts },
                                             { u"headerChecks"_s, headerChecks },
                                             { u"costs"_s, costs },
                                             { u"released"_s, released },
                                             { u"canvasSyncCpuNs"_s,
                                               double(canvasTitleStats().syncNs.load()) },
                                             { u"canvasPaintCpuNs"_s,
                                               double(canvasTitleStats().paintNs.load()) },
                                             { u"canvasPaints"_s,
                                               double(canvasTitleStats().paints.load()) },
                                             { u"windowPolicy"_s,
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
            root->setProperty("night", test[u"night"_s].toBool());
            const QSize size(test[u"width"_s].toInt() + 2, test[u"budget"_s].toInt() + 2);
            window.resize(size);
            root->setSize(size);
            controller.setAvailableWidth(test[u"width"_s].toDouble());
            controller.setBudget(test[u"budget"_s].toDouble());
            controller.setPixelSize(test[u"size"_s].toDouble());
            controller.setText(test[u"text"_s].toString());
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
            root->setProperty("lastAction", u""_s);
            session.setPlaybackIds(u"album-0"_s, u"track-0"_s);
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
            if (!OffscreenSurface::Enabled && qgetenv("QSG_RENDER_LOOP") == "basic"_ba) {
                restorePending = false;
                window.destroy();
                QTimer::singleShot(0, &window, [&] {
                    window.create();
                    surface.show();
                    root->setProperty("marker", ++marker);
                    surface.requestFrame();
                });
            }
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
    // Bound the complete own-render, animation and lifecycle verification.
    QTimer::singleShot(int(90000 * std::max(1.0, dpr * dpr)), &app, [&] {
        QSaveFile file(output + "/timeout.json"_L1);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(QJsonObject{ { u"passed"_s, false },
                                                  { u"step"_s, step },
                                                  { u"staticSteps"_s, staticSteps },
                                                  { u"animationSteps"_s, animationSteps },
                                                  { u"headerSteps"_s, headerSteps },
                                                  { u"warmFrames"_s, warmFrames },
                                                  { u"requested"_s, requested.load() },
                                                  { u"inFlight"_s, inFlight.load() },
                                                  { u"drain"_s, drain.load() },
                                                  { u"visible"_s, window.isVisible() },
                                                  { u"exposed"_s, window.isExposed() },
                                                  { u"restorePending"_s, restorePending.load() },
                                                  { u"dpr"_s, dpr },
                                                  { u"windowPolicy"_s,
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
