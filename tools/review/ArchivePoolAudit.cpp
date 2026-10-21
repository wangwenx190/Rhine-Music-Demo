#include "ReviewSupport.h"
#include "ReviewApplication.h"
#include "SceneAdapter.h"
#include "MotionController.h"
#include "SelectionController.h"
#include "CoverCache.h"
#include <QScopeGuard>
#include "SceneItem.h"
#include "ShellMaterial.h"
#include "GraphicsBackend.h"
#include "VerificationWindow.h"
#include "OffscreenSurface.h"
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
#include <QKeyEvent>
#include <QSet>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <cstdio>
#include <atomic>

using namespace Qt::StringLiterals;

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
            album.id = u"%1:%2"_s.arg(lane).arg(row);
            album.genreId = genre.id;
            album.title = u"\u975e\u79c1\u4eba\u9a8c\u8bc1\u4e13\u8f91 %1"_s.arg(row);
            const QStringList covers{ u"square"_s, u"portrait"_s, u"landscape"_s };
            album.coverUrl = QUrl(u"qrc:/assets/demo-covers/%1.png"_s.arg(covers[row % 3]));
            result.albums.append(album);
        }
    }
    return std::make_shared<const LibrarySnapshot>(std::move(result));
}
}

namespace {

struct Wait final
{
    quint64 request = 0;
    QPointer<QEventLoop> loop;
    std::optional<Capture> capture;
};
auto runArchivePoolAuditCapture(SceneItem *&item, std::shared_ptr<Wait> &state,
                                OffscreenSurface &surface, QQuickWindow &window,
                                const QString &output, QString name, std::optional<Input> supplied)
{
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
    surface.requestFrame();
    timeout.start(yalahoo::review::WorkerDrainDeadlineMilliseconds);
    loop.exec();
    state->loop = nullptr;
    require(state->capture.has_value(), "Pool capture timed out");
    if (!verification::windowPolicySatisfied(window)) {
        QSaveFile failure(output + "/window-policy-failure.json"_L1);
        if (failure.open(QIODevice::WriteOnly)) {
            failure.write(QJsonDocument(verification::windowPolicy(window)).toJson());
            failure.commit();
        }
        throw std::runtime_error("Pool window policy violated");
    }
    auto result = std::move(*state->capture);
    state->capture.reset();
    result.atlas = { };
    require(result.metadata[u"drawCount"_s].toInt() <= 436, "Pool exceeded bounded draw work");
    require(result.metadata[u"atlasUploadBytes"_s].toInteger() <= AtlasUploadQueue::steadyBudget,
            "Pool upload exceeded steady budget");
    const auto native = result.metadata[u"nativeAtlasTransfers"_s].toObject();
    if (native[u"available"_s].toBool())
        require(native[u"destinationBytes"_s].toInteger()
                        == result.metadata[u"atlasUploadBytes"_s].toInteger(),
                "Native pool transfer differs from submitted regions");
    require(item->errorText().isEmpty() && !result.postprocessed.isNull(),
            "Pool renderer failed or returned an empty target");
    const auto uniforms = result.metadata[u"submittedShellUniforms"_s].toArray();
    require(result.metadata[u"visibleEntities"_s].toInt() == 0 || !uniforms.isEmpty(),
            "Visible shells have no submitted material uniforms");
    for (const auto &value : uniforms) {
        const auto sample = value.toObject();
        const auto expected = shellMaterial(
                sample[u"surface"_s].toInt(), float(sample[u"warmth"_s].toDouble()),
                float(sample[u"clarity"_s].toDouble()), float(sample[u"environment"_s].toDouble()));
        const auto check = [&](const char *key, const auto &vector, int size) {
            const auto actual = sample[QString::fromUtf8(key)].toArray();
            require(actual.size() == size, "Missing material uniform channels");
            for (int i = 0; i < size; ++i)
                require(std::abs(actual[i].toDouble() - vector[i]) <= 2e-6,
                        "Submitted material uniform differs from effective source parameter");
        };
        check("transport", expected.transport, 4);
        check("coat", expected.coat, 4);
        check("tint", expected.tint, 3);
        check("attenuation", expected.attenuation, 3);
    }
    require(result.postprocessed.save(output + '/'_L1 + name + ".png"_L1),
            "Cannot save own-render pool image");
    return result;
}

auto runArchivePoolAuditObserveFrame(const QPointer<QQuickWindow> &guard,
                                     const std::shared_ptr<Wait> &state, Capture capture)
{
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
}

auto runArchivePoolAuditWaitPrepared(SceneItem *&item, SceneAdapter &adapter,
                                     OffscreenSurface &surface)
{
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool ready = false;
    const auto check = [&] {
        const auto stats = item->coverStatistics();
        ready = item->input().poolFrame && item->input().scene.revision == adapter.frame().revision
                && stats[u"identities"_s].toInt() == stats[u"ready"_s].toInt()
                && (stats[u"owned"_s].toInt() == 0 || stats[u"selectedDetailed"_s].toBool());
        if (ready)
            loop.quit();
    };
    QObject::connect(item, &SceneItem::frameSubmitted, &loop, check);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    check();
    if (ready)
        return;
    surface.requestFrame();
    timeout.start(yalahoo::review::DefaultDeadlineMilliseconds);
    loop.exec();
    require(ready, "Pool cover preparation timed out");
}

auto runArchivePoolAudit(int &argc, char **&argv, QGuiApplication &app)
{
    const auto coverShutdown = qScopeGuard([] {
        yalahoo::CoverCache::finishDecoders();
    });
    if (qEnvironmentVariableIntValue("YALAHOO_VERIFY_VISIBLE") == 1)
        OffscreenSurface::Enabled = false;
    if (argc != 2 && argc != 3)
        return 2;
    const bool real = argc == 3 && QString::fromLocal8Bit(argv[2]) == "--real-columns"_L1;
    if (argc == 3 && !real)
        return 2;
    try {
        const QString output = QString::fromLocal8Bit(argv[1]);
#if defined(Q_OS_WIN)
        const QString platform = u"windows"_s;
#elif defined(Q_OS_MACOS)
        const QString platform = u"macos"_s;
#else
        const QString platform = u"linux"_s;
#endif
        const QString sourceRoot = qEnvironmentVariable(
                "YALAHOO_POOL_GOLDEN_ROOT",
                (QString::fromUtf8(FIXTURE_ROOT) + u"/verification/"_s) + platform);
        require(QDir().mkpath(output), "Cannot create pool output");
        MotionController motion;
        SelectionController selection(&motion);
        InputRouter inputRouter(&selection);
        SceneAdapter adapter(motion.clock());
        auto snapshot = library(10000);
        if (real) {
            const QList<int> counts{ 0, 1, 2, 3, 49, 106 };
            QList<LibraryColumn> columns;
            LibrarySnapshot finite;
            for (int lane = 0; lane < counts.size(); ++lane) {
                LibraryColumn column;
                column.key = QString::number(lane);
                for (int row = 0; row < counts[lane]; ++row) {
                    const auto album = snapshot->albums[lane * 10000 + row];
                    column.albumIds.append(album.id);
                    finite.albums.append(album);
                }
                columns.append(column);
            }
            snapshot = std::make_shared<const LibrarySnapshot>(std::move(finite));
            selection.setRealistic(true);
            selection.setColumns(columns, snapshot);
        } else
            selection.setColumns(orderLibrary(*snapshot, SortMode::Genre).columns, snapshot);
        adapter.setSelection(&selection);
        adapter.layout()->setViewport(960, 640);
        std::atomic<int> invalidations = 0;
        QQuickRenderControl control;
        QQuickWindow window(OffscreenSurface::Enabled ? &control : nullptr);
        verification::prepareWindow(window, true);
        window.resize(960, 640);
        window.setPersistentSceneGraph(false);
        window.setPersistentGraphics(false);
        OffscreenSurface surface(window, control);
        QObject::connect(
                &window, &QQuickWindow::sceneGraphInvalidated, &window,
                [&] {
                    ++invalidations;
                },
                Qt::DirectConnection);
        auto *item = new SceneItem(window.contentItem());
        item->setSize({ 960, 640 });
        item->setAdapter(&adapter);
        item->setInputRouter(&inputRouter);
        item->setTelemetryEnabled(true);
        QJsonArray submissionSamples;
        QObject::connect(item, &SceneItem::frameSubmitted, &app, [&](QJsonObject sample) {
            if (submissionSamples.size() == 256)
                submissionSamples.removeFirst();
            submissionSamples.append(sample);
        });

        auto state = std::make_shared<Wait>();
        QPointer<QQuickWindow> guard(&window);
        item->captured = [state, guard](Capture capture) {
            return runArchivePoolAuditObserveFrame(guard, state, capture);
        };
        auto waitPrepared = [&] {
            return runArchivePoolAuditWaitPrepared(item, adapter, surface);
        };
        auto capture = [&](QString name, std::optional<Input> supplied = std::nullopt) {
            return runArchivePoolAuditCapture(item, state, surface, window, output, name, supplied);
        };
        surface.show();
        auto settledCapture = [&](QString name) {
            auto result = capture(name);
            int frames = 0;
            while (result.metadata[u"atlasPending"_s].toInt() > 0) {
                require(++frames <= 70, "Bounded pool upload did not drain");
                result = capture(name);
            }
            return result;
        };
        waitPrepared();
        // Drain accepted chunks through real renderer frames, never resubmit in a millisecond loop.
        settledCapture(u"initial"_s);
        const auto stable = capture(u"stable"_s);
        require(stable.metadata[u"atlasUploadBytes"_s].toInteger() == 0,
                "Stable pool uploaded covers again");
        if (real) {
            QFile source(sourceRoot + "/real-columns/frozen-fields.json"_L1);
            require(source.open(QIODevice::ReadOnly), "Rebuild frozen finite-column field input");
            const auto fields =
                    QJsonDocument::fromJson(source.readAll()).object()[u"fields"_s].toArray();
            QHash<QString, QJsonObject> byCell;
            for (auto value : fields) {
                const auto field = value.toObject();
                byCell.insert(u"%1:%2"_s.arg(field[u"lane"_s].toInt()).arg(field[u"row"_s].toInt()),
                              field);
            }
            QSet<QString> identities;
            const auto &sourceInput = item->input();
            double matrixError = 0;
            for (int index = 0; index < sourceInput.scene.instances.size(); ++index) {
                const auto &instance = sourceInput.scene.instances[index];
                require(!identities.contains(instance.albumId), "Finite album repeated");
                identities.insert(instance.albumId);
                const auto key =
                        u"%1:%2"_s.arg(int(instance.cell.lane)).arg(int(instance.cell.row));
                require(byCell.contains(key), "Nonexistent finite cell was drawn");
                const auto field = byCell.value(key);
                const double lift = instance.owned ? .9 : 0;
                QMatrix4x4 pose;
                pose.translate(
                        float((instance.cell.lane - 1) * 5.2),
                        float(-4.6 + field[u"height"_s].toDouble() + lift),
                        float((instance.cell.row - field[u"depth"_s].toDouble()) * .62 - 2.17));
                const double pitch = lift > 0 ? 0 : field[u"slope"_s].toDouble() * .024;
                QMatrix4x4 rotation;
                rotation(1, 1) = float(std::cos(pitch));
                rotation(1, 2) = float(-std::sin(pitch));
                rotation(2, 1) = float(std::sin(pitch));
                rotation(2, 2) = float(std::cos(pitch));
                pose *= rotation;
                for (int component = 0; component < 16; ++component)
                    matrixError = std::max(
                            matrixError,
                            std::abs(double(pose.constData()[component]
                                            - sourceInput.entities[index].constData()[component])));
            }
            require(matrixError <= 1e-6, "Finite transform differs from frozen field");
            const auto generation = stable.metadata[u"resourceGeneration"_s];
            QKeyEvent boundary(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
            QCoreApplication::sendEvent(item, &boundary);
            require(boundary.isAccepted() && selection.albumId() == "1:0"_L1,
                    "Single-column keyboard boundary wrapped");
            selection.setLift(0);
            require(selection.select(u"5:105"_s), "Cannot select last finite album");
            motion.finish();
            selection.advance(2.4);
            waitPrepared();
            const auto last = settledCapture(u"finite-last"_s);
            require(last.metadata[u"resourceGeneration"_s] == generation,
                    "Finite visibility rebuilt targets");
            require(!selection.moveRow(1) && !selection.navigateRow(118),
                    "Finite tail navigation wrapped");
            QWheelEvent wheel({ 480, 320 }, { 480, 320 }, { }, { 0, -120 }, Qt::NoButton,
                              Qt::NoModifier, Qt::NoScrollPhase, false);
            QCoreApplication::sendEvent(item, &wheel);
            require(wheel.isAccepted() && selection.albumId() == "5:105"_L1,
                    "Finite wheel boundary wrapped");
            require(selection.moveLane(-1), "Finite column navigation failed");
            selection.advance(.2);
            const double unfinished = selection.browseRow(5);
            selection.setRememberColumnPosition(false);
            require(selection.moveLane(1), "Cannot return to finite recovering column");
            require(selection.cell().row == std::floor(unfinished + .5),
                    "Return ignored actual column depth");
            motion.finish();
            selection.advance(3);
            waitPrepared();
            settledCapture(u"finite-return"_s);
            adapter.lighting()->setNight(true);
            adapter.lighting()->advanceTheme(2);
            const auto night = settledCapture(u"finite-night"_s);
            require(night.metadata[u"resourceGeneration"_s] == generation,
                    "Theme rebuilt finite targets");
            const int beforeInvalidation = invalidations.load();
            surface.hide();
            require(!OffscreenSurface::Enabled || invalidations > beforeInvalidation,
                    "Finite offscreen scene graph was not invalidated");
            window.releaseResources();
            surface.show();
            waitPrepared();
            const auto restored = settledCapture(u"finite-restored"_s);
            require(restored.metadata[u"drawCount"_s] == night.metadata[u"drawCount"_s]
                            && restored.metadata[u"atlasPending"_s].toInt() == 0,
                    "Finite restore lost geometry or cover uploads");
            selection.setColumns({ });
            const auto empty = settledCapture(u"finite-empty"_s);
            require(empty.metadata[u"drawCount"_s].toInt() == 0,
                    "Finite empty library retained draws");
            QJsonObject report{ { u"passed"_s, true },
                                { u"realColumns"_s, true },
                                { u"sourceMatrixError"_s, matrixError },
                                { u"stable"_s, stable.metadata },
                                { u"last"_s, last.metadata },
                                { u"night"_s, night.metadata },
                                { u"empty"_s, empty.metadata },
                                { u"covers"_s, item->coverStatistics() },
                                { u"windowPolicy"_s, verification::windowPolicy(window) },
                                { u"offscreen"_s, surface.diagnostics() },
                                { u"submissionSamples"_s, submissionSamples } };
            QSaveFile reportFile(output + "/runtime.json"_L1);
            require(reportFile.open(QIODevice::WriteOnly), "Cannot write finite audit");
            reportFile.write(QJsonDocument(report).toJson());
            require(reportFile.commit(), "Cannot commit finite audit");
            return 0;
        }
        require(stable.metadata[u"drawCount"_s].toInt() == 432,
                "Large library changed pool capacity");
        require(stable.metadata[u"visibleEntities"_s].toInt() < 432,
                "Pool visibility was not culled");
        const auto generation = stable.metadata[u"resourceGeneration"_s];
        require(!generation.isUndefined(), "Missing renderer resource generation");
        const auto copies = stable.metadata[u"coverSnapshotCopies"_s].toInteger();
        QFile frozen(sourceRoot + "/archive-pool/frozen-poses.json"_L1);
        require(frozen.open(QIODevice::ReadOnly),
                "Rebuild frozen production poses before the GPU audit");
        const auto poses = QJsonDocument::fromJson(frozen.readAll()).object()[u"poses"_s].toArray();
        require(poses.size() == 432, "Frozen pool pose count differs");
        QHash<QString, QMatrix4x4> expected;
        for (auto value : poses) {
            const auto p = value.toObject();
            const auto values = p[u"matrix"_s].toArray();
            QMatrix4x4 matrix;
            for (int i = 0; i < 16; ++i)
                matrix(i % 4, i / 4) = float(values[i].toDouble());
            expected.insert(u"%1:%2"_s.arg(p[u"lane"_s].toDouble()).arg(p[u"row"_s].toDouble()),
                            matrix);
        }
        const auto &sourceInput = item->input();
        double maxMatrixError = 0;
        for (int index = 0; index < sourceInput.scene.instances.size(); ++index) {
            const auto c = sourceInput.scene.instances[index].cell;
            const auto key = u"%1:%2"_s.arg(c.lane).arg(c.row);
            require(expected.contains(key), "Production cell differs from frozen pool");
            const auto matrix = expected.value(key);
            for (int i = 0; i < 16; ++i)
                maxMatrixError = std::max(maxMatrixError,
                                          std::abs(double(sourceInput.entities[index].constData()[i]
                                                          - matrix.constData()[i])));
        }
        require(maxMatrixError < 1e-5, "Production transform differs from frozen source");
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
        const auto travel = capture(u"travel"_s);
        require(travel.metadata[u"resourceGeneration"_s] == generation,
                "Navigation rebuilt render targets");
        require(travel.metadata[u"coverSnapshotCopies"_s].toInteger() == copies,
                "Navigation deep-copied renderer covers");
        adapter.setReducedMotion(true);
        adapter.setDetail(1);
        adapter.snapToTarget();
        selection.setLift(4.05);
        waitPrepared();
        const auto pixel = adapter.projectWorldPoint(
                (selection.selectedPosition() + SceneVector{ 0, 1.85, 0 }).qt());
        const auto hit = adapter.hitTest(pixel.x(), pixel.y());
        require(hit[u"albumId"_s].toString() == selection.albumId(),
                "Pool selection hit differs from rendered coordinates");
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(pixel.x(), pixel.y()),
                          QPointF(pixel.x(), pixel.y()), Qt::LeftButton, Qt::LeftButton,
                          Qt::NoModifier);
        QCoreApplication::sendEvent(item, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(pixel.x(), pixel.y()),
                            QPointF(pixel.x(), pixel.y()), Qt::LeftButton, Qt::NoButton,
                            Qt::NoModifier);
        QCoreApplication::sendEvent(item, &release);
        require(press.isAccepted() && release.isAccepted(),
                "Production ray click did not accept selected cover");
        const auto detail = capture(u"detail"_s);
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
        const auto returning = capture(u"returning-night"_s);
        require(item->coverStatistics()[u"owned"_s].toInt() == 4,
                "Three returning covers not pinned");
        require(returning.metadata[u"resourceGeneration"_s] == generation,
                "Visible count changed target/pipeline allocation");
        motion.setReducedMotion(true);
        selection.advance(.2);
        waitPrepared();
        const auto restored = capture(u"restored-night"_s);
        if (item->coverStatistics()[u"owned"_s].toInt() != 1) {
            std::fprintf(
                    stderr,
                    "Return mismatch: cases=%lld cpuOwners=%d inputFrame=%llu sceneFrame=%llu\n",
                    qlonglong(selection.returningCases().size()),
                    item->coverStatistics()[u"owned"_s].toInt(),
                    qulonglong(item->input().scene.revision), qulonglong(adapter.frameRevision()));
            for (const auto &copy : selection.returningCases())
                std::fprintf(stderr, "copy row=%g lift=%g yaw=%g hold=%d\n", copy.cell.row,
                             copy.lift.value, copy.rotation, copy.returnY.has_value());
        }
        require(item->coverStatistics()[u"owned"_s].toInt() == 1,
                "Returning covers did not release");
        const auto coverMemory = item->coverStatistics();
        selection.setLift(0);
        require(selection.selectCell({ 3002, 40997 }, true), "Cannot test far coordinates");
        selection.setLift(4.05);
        adapter.updateLighting(0);
        waitPrepared();
        const auto far = settledCapture(u"far-night"_s);
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
        const auto rebased = settledCapture(u"rebased-night"_s);
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
        require(rebaseMatrixError <= 1e-4 && changedCoverRegions == 0 && changedObjects == 0,
                "Rebase changed physical transforms, cover regions or optical identities");
        require(std::abs(farInput.focus - rebasedInput.focus) <= 1e-4
                        && (farInput.rowColumn - rebasedInput.rowColumn).length() <= 1e-4,
                "Rebase changed focal depth or world-space lighting");
        const int beforeInvalidation = invalidations.load();
        surface.hide();
        require(!OffscreenSurface::Enabled || invalidations > beforeInvalidation,
                "Offscreen scene graph was not invalidated");
        window.releaseResources();
        surface.show();
        waitPrepared();
        auto rebuild = settledCapture(u"rebuild-night"_s);
        require(rebuild.metadata[u"drawCount"_s] == rebased.metadata[u"drawCount"_s]
                        && rebuild.metadata[u"atlasPending"_s].toInt() == 0,
                "Context rebuild lost pool geometry or cover uploads");
        const int lifecycleCycles =
                qEnvironmentVariableIntValue("YALAHOO_VERIFY_LIFECYCLE") == 1 ? 10 : 1;
        for (int cycle = 1; cycle < lifecycleCycles; ++cycle) {
            const int previousInvalidations = invalidations.load();
            surface.hide();
            require(!OffscreenSurface::Enabled || invalidations > previousInvalidations,
                    "Repeated offscreen scene graph was not invalidated");
            window.releaseResources();
            surface.show();
            waitPrepared();
            rebuild = settledCapture(u"rebuild-night-%1"_s.arg(cycle + 1));
            require(rebuild.metadata[u"drawCount"_s] == rebased.metadata[u"drawCount"_s]
                            && rebuild.metadata[u"atlasPending"_s].toInt() == 0,
                    "Repeated context rebuild lost geometry or cover uploads");
        }
        // R22: exercise revision changes through the actual production atlas/renderer.
        const auto columns = orderLibrary(*snapshot, SortMode::Genre).columns;
        selection.setColumns(columns, snapshot);
        waitPrepared();
        settledCapture(u"cover-reuse-baseline"_s);
        const auto reuseBaseline = capture(u"cover-reuse-stable"_s);
        const auto decodedBefore =
                item->coverStatistics().value(u"cache"_s).toObject().value(u"decodeStarts"_s);
        auto changedLibrary = *snapshot;
        auto changed = std::find_if(changedLibrary.albums.begin(), changedLibrary.albums.end(),
                                    [&](const Album &album) {
                                        return album.id == selection.albumId();
                                    });
        require(changed != changedLibrary.albums.end(), "Selected cover identity missing");
        Track track;
        track.id = "anonymous-other-song"_L1;
        track.albumId = changed->id;
        changed->tracks.append(track);
        const auto albumIndex = std::distance(changedLibrary.albums.begin(), changed);
        qint64 revisionBytes = 0;
        const auto observe = QObject::connect(
                item, &SceneItem::frameSubmitted, &app, [&](const QJsonObject &sample) {
                    revisionBytes += sample.value(u"atlasUploadBytes"_s).toInteger();
                });
        selection.setColumns(columns, std::make_shared<const LibrarySnapshot>(changedLibrary));
        waitPrepared();
        const auto sameAlbum = capture(u"same-album-other-song"_s);
        require(sameAlbum.metadata[u"atlasUploadBytes"_s].toInteger() == 0 && revisionBytes == 0,
                "Same album song change uploaded unchanged covers");
        require(item->coverStatistics().value(u"cache"_s).toObject().value(u"decodeStarts"_s)
                        == decodedBefore,
                "Same album song change decoded unchanged covers");
        auto &changedAlbum = changedLibrary.albums[albumIndex];
        changedAlbum.coverUrl = changedAlbum.coverUrl.path().endsWith(u"square.png"_s)
                ? QUrl(u"qrc:/assets/demo-covers/portrait.png"_s)
                : QUrl(u"qrc:/assets/demo-covers/square.png"_s);
        selection.setColumns(columns,
                             std::make_shared<const LibrarySnapshot>(std::move(changedLibrary)));
        waitPrepared();
        settledCapture(u"changed-cover-revision"_s);
        const auto changedCover = capture(u"changed-cover-stable"_s);
        QObject::disconnect(observe);
        const auto current = item->input();
        const int selected = int(current.entities.size()) - 1;
        qint64 expectedBytes = 0;
        for (const auto &upload : current.atlasUploads) {
            const QPoint origin((upload.slot % (current.atlasSize.width() / 256)) * 256,
                                (upload.slot / (current.atlasSize.width() / 256)) * 256);
            if (current.coverTiles[selected].contains(origin)
                || current.fallbackCoverTiles[selected].contains(origin))
                expectedBytes += AtlasUploadQueue::payloadBytes(upload);
        }
        require(revisionBytes == expectedBytes,
                "Cover revision must upload only changed thumbnail/detail regions and their mips");
        require(changedCover.metadata[u"atlasUploadBytes"_s].toInteger() == 0,
                "Changed cover did not settle to zero uploads");
        for (const auto *result : { &sameAlbum, &changedCover }) {
            require(result->metadata[u"resourceGeneration"_s]
                                    == reuseBaseline.metadata[u"resourceGeneration"_s]
                            && result->metadata[u"coverResourceRevision"_s]
                                    == reuseBaseline.metadata[u"coverResourceRevision"_s],
                    "Content revision rebuilt compatible render/cover resources");
        }
        QJsonObject coverReuse{ { u"sameAlbum"_s, sameAlbum.metadata },
                                { u"changedCover"_s, changedCover.metadata },
                                { u"revisionUploadBytes"_s, revisionBytes } };
        auto delayedHigh = item->input();
        const int selectedIndex = int(delayedHigh.entities.size()) - 1;
        require(delayedHigh.coverTiles[selectedIndex].width() >= 256,
                "Selected high-resolution region not ready");
        delayedHigh.coverLeases[selectedIndex] += quint64(1) << 40;
        const auto fallback = capture(u"delayed-high-fallback"_s, delayedHigh);
        auto lowOnly = delayedHigh;
        lowOnly.coverTiles[selectedIndex] = lowOnly.fallbackCoverTiles[selectedIndex];
        lowOnly.coverLeases[selectedIndex] = lowOnly.fallbackCoverLeases[selectedIndex];
        const auto low = capture(u"low-oracle"_s, lowOnly);
        const auto fallbackRegion = fallback.metadata[u"effectiveCoverRegions"_s].toArray();
        const auto lowRegion = low.metadata[u"effectiveCoverRegions"_s].toArray();
        require(fallbackRegion.size() == delayedHigh.entities.size() && fallbackRegion == lowRegion
                        && fallbackRegion[selectedIndex].toObject()[u"width"_s].toInt() == 256,
                "Unready high-resolution region did not use the ready thumbnail");
        selection.setColumns({ });
        waitPrepared();
        const auto empty = capture(u"empty"_s);
        require(empty.metadata[u"drawCount"_s].toInt() == 0,
                "Empty library rendered a phantom album");
        QJsonObject report{
            { u"passed"_s, true },
            { u"libraryAlbums"_s, snapshot->albums.size() },
            { u"configuration"_s, u"Release"_s },
            { u"sourceMatrixError"_s, maxMatrixError },
            { u"rebaseMatrixError"_s, rebaseMatrixError },
            { u"lifecycleCycles"_s, lifecycleCycles },
            { u"windowPolicy"_s, verification::windowPolicy(window) },
            { u"offscreen"_s, surface.diagnostics() },
            { u"stable"_s, stable.metadata },
            { u"travel"_s, travel.metadata },
            { u"detail"_s, detail.metadata },
            { u"returning"_s, returning.metadata },
            { u"restored"_s, restored.metadata },
            { u"far"_s, far.metadata },
            { u"rebased"_s, rebased.metadata },
            { u"coverMemory"_s, coverMemory },
            { u"coverReuse"_s, coverReuse },
            { u"submissionSamples"_s, submissionSamples },
            { u"rebuild"_s, rebuild.metadata },
            { u"empty"_s, empty.metadata },
            { u"scope"_s,
              u"Production pool matrices, picking, uploads, optical identities and resource/lifecycle; own-render images for quality inspection, no image matching or presentation performance"_s }
        };
        QSaveFile file(output + "/runtime.json"_L1);
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
}
int main(int argc, char **argv)
{
    return yalahoo::review::runApplication(argc, argv, [&](QGuiApplication &app) {
        return runArchivePoolAudit(argc, argv, app);
    });
}
