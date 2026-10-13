#include "SceneItem.h"
#include "GraphicsBackend.h"
#include "VerificationWindow.h"
#include "ImageComparison.h"
#include <QDir>
#include <QEventLoop>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QPointer>
#include <QQuickWindow>
#include <QSaveFile>
#include <QTimer>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFile>
#include <optional>
#include <stdexcept>
#include <cstdio>
using namespace yalahoo;
using namespace yalahoo::rhi;
namespace {
void require(bool condition, const char *reason)
{
    if (!condition)
        throw std::runtime_error(reason);
}
Snapshot library(int count)
{
    LibrarySnapshot result;
    for (int lane = 0; lane < 9; ++lane) {
        Genre genre;
        genre.id = QString::number(lane);
        genre.name = genre.id;
        result.genres.append(genre);
        for (int row = 0; row < count; ++row) {
            Album album;
            album.id = QStringLiteral("%1:%2").arg(lane).arg(row);
            album.genreId = genre.id;
            album.title = QStringLiteral("非私人验证专辑 %1").arg(row);
            const QStringList covers{ "square", "portrait", "landscape" };
            album.coverUrl =
                    QUrl(QStringLiteral("qrc:/assets/demo-covers/%1.png").arg(covers[row % 3]));
            result.albums.append(album);
        }
    }
    return std::make_shared<const LibrarySnapshot>(std::move(result));
}
}
int main(int argc, char **argv)
{
    if (!selectGraphicsBackend())
        return 2;
    QGuiApplication app(argc, argv);
    if (argc != 2)
        return 2;
    try {
        const QString output = QString::fromLocal8Bit(argv[1]);
        require(QDir().mkpath(output), "Cannot create pool output");
        MotionController motion;
        SelectionController selection(&motion);
        SceneAdapter adapter(motion.clock());
        auto snapshot = library(10000);
        selection.setColumns(orderLibrary(*snapshot, SortMode::Genre).columns, snapshot);
        adapter.setSelection(&selection);
        adapter.layout()->setViewport(960, 640);
        QQuickWindow window;
        verification::prepareWindow(window, true);
        window.resize(960, 640);
        window.setPersistentSceneGraph(false);
        window.setPersistentGraphics(false);
        auto *item = new SceneItem(window.contentItem());
        item->setSize({ 960, 640 });
        item->setAdapter(&adapter);
        item->setTelemetryEnabled(true);
        QJsonArray submissionSamples;
        QObject::connect(item, &SceneItem::frameSubmitted, &app, [&](QJsonObject sample) {
            if (submissionSamples.size() == 256)
                submissionSamples.removeFirst();
            submissionSamples.append(sample);
        });
        struct Wait
        {
            quint64 request = 0;
            QPointer<QEventLoop> loop;
            std::optional<Capture> capture;
        };
        auto state = std::make_shared<Wait>();
        QPointer<QQuickWindow> guard(&window);
        item->captured = [state, guard](Capture capture) {
            if (guard)
                QMetaObject::invokeMethod(
                        guard,
                        [state, capture = std::move(capture)]() mutable {
                            if (state->loop && capture.request == state->request) {
                                state->capture = std::move(capture);
                                state->loop->quit();
                            }
                        },
                        Qt::QueuedConnection);
        };
        auto waitPrepared = [&] {
            QEventLoop loop;
            QTimer check, timeout;
            timeout.setSingleShot(true);
            bool ready = false;
            QObject::connect(&check, &QTimer::timeout, &loop, [&] {
                const auto stats = item->coverStatistics();
                ready = item->input().poolFrame
                        && item->input().scene.revision == adapter.frame().revision
                        && stats["identities"].toInt() == stats["ready"].toInt()
                        && (stats["owned"].toInt() == 0 || stats["selectedDetailed"].toBool());
                if (ready)
                    loop.quit();
            });
            QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
            check.start(50);
            timeout.start(60000);
            loop.exec();
            require(ready, "Pool cover preparation timed out");
        };
        auto capture = [&](QString name, std::optional<Input> supplied = std::nullopt) {
            std::fprintf(stderr, "Pool capture: %s\n", name.toUtf8().constData());
            QEventLoop loop;
            QTimer timeout;
            timeout.setSingleShot(true);
            QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
            auto input = supplied.value_or(item->input());
            input.captureAtlas = false;
            input.request = ++state->request;
            state->capture.reset();
            state->loop = &loop;
            item->submit(std::move(input),
                         { item->fixedColorBufferWidth(), item->fixedColorBufferHeight() });
            timeout.start(30000);
            loop.exec();
            state->loop = nullptr;
            require(state->capture.has_value(), "Pool capture timed out");
            require(verification::windowPolicySatisfied(window), "Pool window policy violated");
            auto result = std::move(*state->capture);
            state->capture.reset();
            result.atlas = { };
            require(result.metadata["drawCount"].toInt() <= 436, "Pool exceeded bounded draw work");
            require(result.metadata["atlasUploadBytes"].toInteger()
                            <= AtlasUploadQueue::steadyBudget,
                    "Pool upload exceeded steady budget");
            const auto native = result.metadata["nativeAtlasTransfers"].toObject();
            if (native["available"].toBool())
                require(native["destinationBytes"].toInteger()
                                == result.metadata["atlasUploadBytes"].toInteger(),
                        "Native pool transfer differs from submitted regions");
            result.postprocessed.save(output + '/' + name + ".png");
            return result;
        };
        window.show();
        auto settledCapture = [&](QString name) {
            auto result = capture(name);
            int frames = 0;
            while (result.metadata["atlasPending"].toInt() > 0) {
                require(++frames <= 70, "Bounded pool upload did not drain");
                result = capture(name);
            }
            return result;
        };
        waitPrepared();
        auto initial = capture("initial");
        // Drain accepted chunks through real renderer frames, never resubmit in a millisecond loop.
        while (initial.metadata["atlasPending"].toInt() > 0)
            initial = capture("ready");
        const auto stable = capture("stable");
        require(stable.metadata["atlasUploadBytes"].toInteger() == 0,
                "Stable pool uploaded covers again");
        require(stable.metadata["drawCount"].toInt() == 432, "Large library changed pool capacity");
        require(stable.metadata["visibleEntities"].toInt() < 432, "Pool visibility was not culled");
        const auto generation = stable.metadata["resourceGeneration"];
        require(!generation.isUndefined(), "Missing renderer resource generation");
        const auto copies = stable.metadata["coverSnapshotCopies"].toInteger();
        QFile frozen(QStringLiteral(FIXTURE_ROOT
                                    "/verification/windows/archive-pool/frozen-poses.json"));
        require(frozen.open(QIODevice::ReadOnly),
                "Rebuild frozen production poses before the GPU audit");
        const auto poses = QJsonDocument::fromJson(frozen.readAll()).object()["poses"].toArray();
        require(poses.size() == 432, "Frozen pool pose count differs");
        QHash<QString, QMatrix4x4> expected;
        for (auto value : poses) {
            const auto p = value.toObject();
            const auto values = p["matrix"].toArray();
            QMatrix4x4 matrix;
            for (int i = 0; i < 16; ++i)
                matrix(i % 4, i / 4) = float(values[i].toDouble());
            expected.insert(
                    QStringLiteral("%1:%2").arg(p["lane"].toDouble()).arg(p["row"].toDouble()),
                    matrix);
        }
        auto sourceInput = item->input();
        double maxMatrixError = 0;
        for (int index = 0; index < sourceInput.scene.instances.size(); ++index) {
            const auto c = sourceInput.scene.instances[index].cell;
            const auto key = QStringLiteral("%1:%2").arg(c.lane).arg(c.row);
            require(expected.contains(key), "Production cell differs from frozen pool");
            const auto matrix = expected.value(key);
            for (int i = 0; i < 16; ++i)
                maxMatrixError = std::max(maxMatrixError,
                                          std::abs(double(sourceInput.entities[index].constData()[i]
                                                          - matrix.constData()[i])));
            sourceInput.entities[index] = matrix;
        }
        require(maxMatrixError < 1e-5, "Production transform differs from frozen source");
        const auto source = capture("frozen-source", sourceInput);
        const auto sourceImage =
                compareImages(stable.postprocessed, source.postprocessed,
                              QPolygonF(QRectF(QPointF(), QSizeF(stable.postprocessed.size()))));
        require(sourceImage["valid"].toBool() && sourceImage["ssim"].toDouble() > .999,
                "Production pool pixels differ from frozen transforms");
        const auto before = selection.cell();
        const QPointF mouse(480, 320);
        QWheelEvent wheel(mouse, mouse, { }, { 0, -120 }, Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(item, &wheel);
        require(wheel.isAccepted() && selection.cell().row == before.row + 1,
                "Production wheel did not navigate physically");
        selection.setLift(.9);
        motion.finish();
        selection.advance(0);
        waitPrepared();
        const auto travel = capture("travel");
        require(travel.metadata["resourceGeneration"] == generation,
                "Navigation rebuilt render targets");
        require(travel.metadata["coverSnapshotCopies"].toInteger() == copies,
                "Navigation deep-copied renderer covers");
        adapter.setReducedMotion(true);
        adapter.setDetail(1);
        adapter.snapToTarget();
        selection.setLift(4.05);
        waitPrepared();
        const auto pixel = adapter.projectWorldPoint(
                (selection.selectedPosition() + SceneVector{ 0, 1.85, 0 }).qt());
        const auto hit = adapter.hitTest(pixel.x(), pixel.y());
        require(hit["albumId"].toString() == selection.albumId(),
                "Pool selection hit differs from rendered coordinates");
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(pixel.x(), pixel.y()),
                          QPointF(pixel.x(), pixel.y()), Qt::LeftButton, Qt::LeftButton,
                          Qt::NoModifier);
        QCoreApplication::sendEvent(item, &press);
        require(press.isAccepted(), "Production ray event did not accept selected cover");
        const auto detail = capture("detail");
        motion.setReducedMotion(true);
        selection.advance(0);
        motion.setReducedMotion(false);
        adapter.lighting()->setNight(true);
        adapter.lighting()->advanceTheme(2);
        selection.setRotation(.6);
        require(selection.moveRow(1), "Cannot create returning selection");
        selection.setLift(4.05);
        require(selection.moveRow(1), "Cannot create second returning selection");
        selection.setLift(4.05);
        require(selection.moveRow(1), "Cannot create third returning selection");
        selection.setLift(4.05);
        waitPrepared();
        const auto returning = capture("returning-night");
        require(item->coverStatistics()["owned"].toInt() == 4, "Three returning covers not pinned");
        require(returning.metadata["resourceGeneration"] == generation,
                "Visible count changed target/pipeline allocation");
        motion.setReducedMotion(true);
        selection.advance(.2);
        waitPrepared();
        const auto restored = capture("restored-night");
        require(item->coverStatistics()["owned"].toInt() == 1, "Returning covers did not release");
        const auto coverMemory = item->coverStatistics();
        selection.setLift(0);
        require(selection.selectCell({ 3002, 40997 }, true), "Cannot test far coordinates");
        selection.setLift(4.05);
        adapter.updateLighting(0);
        waitPrepared();
        const auto far = settledCapture("far-night");
        const auto farInput = item->input();
        const auto position = selection.selectedPosition();
        const auto light = adapter.lighting()->rowColumn();
        const auto shift = selection.rebase();
        require(shift.lane != 0 && shift.row != 0, "Far coordinates did not rebase");
        adapter.updateLighting(0);
        require((selection.selectedPosition() - position).length() < 1e-4,
                "Rebase moved selected pixels");
        require((adapter.lighting()->rowColumn() - light).length() < 1e-4,
                "Rebase moved world-space ribbon");
        waitPrepared();
        const auto rebased = settledCapture("rebased-night");
        const auto rebasedInput = item->input();
        double rebaseMatrixError = 0;
        int changedCoverRegions = 0, changedObjects = 0;
        for (int i = 0; i < farInput.scene.instances.size(); ++i) {
            const auto id = farInput.scene.instances[i].albumId;
            int next = -1;
            for (int j = 0; j < rebasedInput.scene.instances.size(); ++j)
                if (rebasedInput.scene.instances[j].albumId == id)
                    next = j;
            if (next < 0)
                std::fprintf(
                        stderr,
                        "Missing after rebase: %s at %.17g,%.17g; shift %.17g,%.17g; selected %.17g,%.17g; counts %d/%d\n",
                        id.toUtf8().constData(), farInput.scene.instances[i].cell.lane,
                        farInput.scene.instances[i].cell.row, shift.lane, shift.row,
                        selection.cell().lane, selection.cell().row,
                        int(farInput.scene.instances.size()),
                        int(rebasedInput.scene.instances.size()));
            require(next >= 0, "Rebase changed a visible album identity");
            for (int k = 0; k < 16; ++k)
                rebaseMatrixError =
                        std::max(rebaseMatrixError,
                                 std::abs(double(farInput.entities[i].constData()[k]
                                                 - rebasedInput.entities[next].constData()[k])));
            changedCoverRegions += farInput.coverTiles[i] != rebasedInput.coverTiles[next];
            changedObjects += farInput.entityIds[i] != rebasedInput.entityIds[next];
        }
        std::fprintf(
                stderr,
                "Rebase matrix %.9g, cover regions %d, objects %d, focus %.9g/%.9g, lights %g\n",
                rebaseMatrixError, changedCoverRegions, changedObjects, farInput.focus,
                rebasedInput.focus, double((farInput.rowColumn - rebasedInput.rowColumn).length()));
        const auto rebaseImage =
                compareImages(far.postprocessed, rebased.postprocessed,
                              QPolygonF(QRectF(QPointF(), QSizeF(far.postprocessed.size()))));
        std::fprintf(stderr, "Rebase image SSIM: %.9f, mean RGB error: %.9f\n",
                     rebaseImage["ssim"].toDouble(), rebaseImage["meanAbsoluteRgb"].toDouble());
        require(rebaseImage["valid"].toBool() && rebaseImage["ssim"].toDouble() > .999,
                "Rebase changed pool pixels");
        window.hide();
        window.releaseResources();
        window.show();
        waitPrepared();
        auto rebuild = capture("rebuild-night");
        while (rebuild.metadata["atlasPending"].toInt() > 0)
            rebuild = capture("rebuilt-night");
        const auto metrics =
                compareImages(rebuild.postprocessed, rebased.postprocessed,
                              QPolygonF(QRectF(QPointF(), QSizeF(rebased.postprocessed.size()))));
        require(metrics["valid"].toBool() && metrics["ssim"].toDouble() > .999,
                "Context rebuild changed pool pixels");
        auto delayedHigh = item->input();
        const int selectedIndex = int(delayedHigh.entities.size()) - 1;
        require(delayedHigh.coverTiles[selectedIndex].size() == QSize(1024, 1024),
                "Selected high-resolution region not ready");
        delayedHigh.coverLeases[selectedIndex] += quint64(1) << 40;
        const auto fallback = capture("delayed-high-fallback", delayedHigh);
        auto lowOnly = delayedHigh;
        lowOnly.coverTiles[selectedIndex] = lowOnly.fallbackCoverTiles[selectedIndex];
        lowOnly.coverLeases[selectedIndex] = lowOnly.fallbackCoverLeases[selectedIndex];
        const auto low = capture("low-oracle", lowOnly);
        const auto fallbackImage =
                compareImages(fallback.postprocessed, low.postprocessed,
                              QPolygonF(QRectF(QPointF(), QSizeF(low.postprocessed.size()))));
        require(fallbackImage["valid"].toBool() && fallbackImage["ssim"].toDouble() > .999,
                "Unready high-resolution region hid the ready thumbnail");
        selection.setColumns({ });
        waitPrepared();
        const auto empty = capture("empty");
        require(empty.metadata["drawCount"].toInt() == 0, "Empty library rendered a phantom album");
        QJsonObject report{
            { "passed", true },
            { "libraryAlbums", snapshot->albums.size() },
            { "configuration", "Release" },
            { "sourceMatrixError", maxMatrixError },
            { "sourceImage", sourceImage },
            { "windowPolicy", verification::windowPolicy(window) },
            { "stable", stable.metadata },
            { "travel", travel.metadata },
            { "detail", detail.metadata },
            { "returning", returning.metadata },
            { "restored", restored.metadata },
            { "far", far.metadata },
            { "rebased", rebased.metadata },
            { "rebaseImage", rebaseImage },
            { "coverMemory", coverMemory },
            { "submissionSamples", submissionSamples },
            { "rebuild", rebuild.metadata },
            { "rebuildImage", metrics },
            { "fallbackImage", fallbackImage },
            { "empty", empty.metadata },
            { "scope",
              "Production pool functionality, uploads, resource/lifecycle and own-render visual checks; no full-product frame-rate acceptance" }
        };
        QSaveFile file(output + "/runtime.json");
        require(file.open(QIODevice::WriteOnly), "Cannot save pool report");
        file.write(QJsonDocument(report).toJson());
        require(file.commit(), "Cannot commit pool report");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Pool audit failure: %s\n", error.what());
        qCritical("%s", error.what());
        return 1;
    }
}
