#include "SelectionController.h"
#include "SceneAdapter.h"
#include "LibrarySession.h"
#include "OfflineMetadataProvider.h"
#include "MotionFunctions.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTest>
#include <QSet>
#include <limits>
using namespace yalahoo;
namespace {
QList<LibraryColumn> columns(const QList<int> &counts)
{
    QList<LibraryColumn> result;
    for (int lane = 0; lane < counts.size(); ++lane) {
        LibraryColumn c;
        for (int row = 0; row < counts[lane]; ++row)
            c.albumIds.append(QStringLiteral("%1:%2").arg(lane).arg(row));
        result.append(c);
    }
    return result;
}
ArchiveCell cell(QJsonObject value)
{
    return { value["lane"].toDouble(), value["row"].toDouble() };
}
}
class ArchivePoolTest : public QObject
{
    Q_OBJECT
    QJsonObject golden;
private slots:
    void reducedFramesReusePublishedInstances()
    {
        MotionController motion;
        SelectionController selection(&motion);
        selection.setColumns(columns({ 3, 3, 3 }));
        motion.setReducedMotion(true);
        selection.advance(0);
        const auto revision = selection.revision();
        motion.clock()->setActive(true);
        motion.clock()->advance(0);
        for (int frame = 0; frame < 30; ++frame)
            motion.clock()->advance(1.0 / 60);
        QCOMPARE(selection.revision(), revision);
        QVERIFY(motion.clock()->visualTime() > 0);
        QVERIFY(selection.moveRow(1));
        selection.advance(0);
        QVERIFY(selection.revision() > revision);
    }
    void inactiveControllerDoesNotOwnMotion()
    {
        MotionController motion;
        SelectionController inactive(&motion);
        motion.travel(-.62, -5.2, 7, 1);
        motion.finish();
        inactive.advance(.5);
        QCOMPARE(motion.rail(), -.62);
        QCOMPARE(motion.column(), -5.2);
        motion.setReducedMotion(true);
        QCOMPARE(motion.rail(), -.62);
        QCOMPARE(motion.column(), -5.2);
    }
    void initTestCase()
    {
        QFile file(QStringLiteral(FIXTURE_ROOT "/tests/fixtures/archive-pool/golden.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        golden = QJsonDocument::fromJson(file.readAll()).object();
    }
    void frozenCellsAndDirections()
    {
        ArchivePool pool;
        QList<int> counts;
        for (auto n : golden["counts"].toArray())
            counts.append(n.toInt());
        pool.setColumns(columns(counts));
        for (auto sample : golden["pool"].toArray()) {
            const auto value = sample.toObject();
            pool.update(cell(value["center"].toObject()));
            QSet<QString> physical;
            for (const auto &instance : pool.instances())
                physical.insert(
                        QStringLiteral("%1:%2").arg(instance.cell.lane).arg(instance.cell.row));
            QCOMPARE(physical.size(), 432);
            for (auto expected : value["cells"].toArray()) {
                const auto e = expected.toObject();
                const auto &actual = pool.instances()[e["index"].toInt()];
                QCOMPARE(actual.cell, cell(e));
                QCOMPARE(actual.albumId, e["albumId"].toString());
            }
        }
        for (auto sample : golden["nearest"].toArray()) {
            const auto value = sample.toObject();
            const auto input = value["input"].toArray();
            QCOMPARE(ArchivePool::nearestOccurrence(input[0].toDouble(), input[1].toDouble(),
                                                    input[2].toDouble()),
                     value["output"].toDouble());
        }
        MotionController motion;
        SelectionController selection(&motion);
        selection.setColumns(columns({ 1, 2, 3, 7, 64, 97, 101, 113, 127 }));
        QVERIFY(selection.selectCell({ 2, 14 }));
        QVERIFY(selection.moveRow(1));
        QCOMPARE(selection.cell(), (ArchiveCell{ 2, 15 }));
        QCOMPARE(selection.albumId(), QStringLiteral("2:0"));
        QVERIFY(selection.moveRow(-1));
        QCOMPARE(selection.cell(), (ArchiveCell{ 2, 14 }));
        QVERIFY(selection.moveLane(1));
        QCOMPARE(selection.cell().lane, 3);
        QVERIFY(selection.select(QStringLiteral("1:0")));
        QCOMPARE(selection.cell().lane, 1);
        QCOMPARE(selection.cell().row, 12);
    }
    void frozenWaveFormulae()
    {
        for (auto sample : golden["fields"].toArray()) {
            const auto v = sample.toObject();
            const double t = v["time"].toDouble(), r = v["row"].toDouble(),
                         l = v["lane"].toDouble();
            QVERIFY(std::abs(archiveWave(r, l, t) - v["archive"].toDouble()) < 1e-10);
            QVERIFY(std::abs(idleWave(r, l, t) - v["idle"].toDouble()) < 1e-10);
            QVERIFY(std::abs(musicSelectionWave(std::abs(r - 12), t) - v["pulse"].toDouble())
                    < 1e-10);
            QVERIFY(std::abs(settlingWave(r - 12, 26.56) * columnStrength(l, 2)
                             - v["resting"].toDouble())
                    < 1e-10);
            QVERIFY(std::abs(extraction(t) - v["extraction"].toDouble()) < 1e-10);
            QVERIFY(std::abs(returnStep(.8, t) - v["returnAngle"].toDouble()) < 1e-10);
        }
    }
    void frozenProductionTransforms()
    {
        MotionController motion;
        SelectionController selection(&motion);
        selection.setColumns(columns({ 1, 2, 3, 7, 64, 97, 101, 113, 127 }));
        for (auto sample : golden["poses"].toArray()) {
            const auto value = sample.toObject();
            const auto targetCell = cell(value);
            const auto it =
                    std::find_if(selection.instances().cbegin(), selection.instances().cend(),
                                 [&](const ArchiveInstance &i) {
                                     return i.cell == targetCell;
                                 });
            QVERIFY(it != selection.instances().cend());
            QCOMPARE(it->albumId, value["albumId"].toString());
            const auto expected = value["matrix"].toArray();
            for (int i = 0; i < 16; ++i)
                QVERIFY(std::abs(it->transform.constData()[i] - expected[i].toDouble()) < 1e-5);
        }
        for (auto sample : golden["ownedPoses"].toArray()) {
            const auto value = sample.toObject();
            auto springs = motion.springs();
            springs[2].value = value["shoulder"].toDouble();
            motion.observe(
                    springs,
                    { springs[0].value, springs[1].value, springs[2].value, springs[3].value },
                    { 3.7, 3.7, 5, 4 });
            selection.setLift(value["lift"].toDouble());
            selection.setRotation(value["rotation"].toDouble());
            const auto expected = value["matrix"].toArray();
            const auto &matrix = selection.instances().last().transform;
            for (int i = 0; i < 16; ++i)
                QVERIFY(std::abs(matrix.constData()[i] - expected[i].toDouble()) < 1e-5);
        }
        for (auto sample : golden["selection"].toArray()) {
            const auto value = sample.toObject();
            selection.setColumns(columns({ 1, 2, 3, 7, 64, 97, 101, 113, 127 }));
            QVERIFY(selection.selectCell(cell(value["current"].toObject())));
            selection.setLift(0);
            const auto navigation = value["navigation"].toObject();
            if (navigation["axis"] == "row")
                QVERIFY(selection.moveRow(navigation["direction"].toInt()));
            else if (navigation["axis"] == "lane")
                QVERIFY(selection.selectCell(
                        { selection.cell().lane + navigation["direction"].toDouble(),
                          std::round(navigation["row"].toDouble()) }));
            else
                QVERIFY(selection.select(value["id"].toString()));
            QCOMPARE(selection.cell(), cell(value["cell"].toObject()));
        }
    }
    void ownershipReturnAndReclaim()
    {
        MotionController motion;
        SelectionController selection(&motion);
        selection.setColumns(columns({ 3, 3, 3 }));
        selection.setLift(4.05);
        selection.setRotation(.7);
        QVERIFY(selection.moveRow(1));
        selection.setLift(.4);
        QVERIFY(selection.moveRow(1));
        selection.setLift(.4);
        QVERIFY(selection.moveRow(1));
        selection.setLift(.4);
        const auto before = selection.cell();
        QVERIFY(!selection.moveRow(1));
        QCOMPARE(selection.cell(), before);
        QVERIFY(selection.selectCell({ 0, 12 }));
        QSet<QString> cells;
        for (const auto &i : selection.instances()) {
            const auto key = QStringLiteral("%1:%2").arg(i.cell.lane).arg(i.cell.row);
            QVERIFY(!cells.contains(key));
            cells.insert(key);
        }
        QCOMPARE(selection.instances().size(), 432);
        selection.returnSelected();
        for (int frame = 0; frame < 600; ++frame)
            selection.advance(1.0 / 60);
        int owned = 0;
        for (const auto &i : selection.instances())
            owned += i.owned;
        QCOMPARE(owned, 1);
        QVERIFY(selection.instances().size() <= 436);
    }
    void rebasePreservesDisplayAndDerivatives()
    {
        MotionController motion;
        SelectionController selection(&motion);
        selection.setColumns(columns({ 2, 3, 4 }));
        SceneAdapter scene;
        scene.setSelection(&selection);
        QVERIFY(selection.selectCell({ 3002, 4097 }, true));
        motion.finish();
        auto active = motion.springs();
        for (int i = 0; i < 4; ++i)
            active[i].velocity = .2 * (i + 1);
        motion.observe(active,
                       { active[0].value + 1, active[1].value - 1, active[2].value + 2,
                         active[3].value - 2 },
                       { 3.7, 3.7, 5, 4 });
        motion.retarget({ active[0].value - 12.4, active[1].value + 10.4, active[2].value + 20,
                          active[3].value + 2 });
        selection.setLift(.4);
        // Publish at the far position before reducing its logical origin.
        selection.setRotation(.1);
        const auto oldPosition = selection.selectedPosition();
        const auto oldAngle = selection.selectedRotation();
        const auto oldId = selection.albumId();
        const auto springs = motion.springs();
        const auto tracks = motion.archive().state();
        const auto oldField = selection.field(selection.cell().row + 1, selection.cell().lane);
        const auto shift = selection.rebase();
        QVERIFY(shift.lane != 0 && shift.row != 0);
        QCOMPARE(selection.albumId(), oldId);
        QVERIFY((selection.selectedPosition() - oldPosition).length() < 1e-4);
        QCOMPARE(selection.selectedRotation(), oldAngle);
        QVERIFY(std::abs(selection.field(selection.cell().row + 1, selection.cell().lane)
                         - oldField)
                < 1e-10);
        QVERIFY(motion.moving());
        for (int i = 0; i < 4; ++i) {
            QCOMPARE(motion.springs()[i].velocity, springs[i].velocity);
            QCOMPARE(motion.springs()[i].acceleration, springs[i].acceleration);
            QCOMPARE(motion.archive().state()[i].velocity, tracks[i].velocity);
            QCOMPARE(motion.archive().state()[i].acceleration, tracks[i].acceleration);
        }
        QCOMPARE(selection.coordinateOrigin(), shift);
        QCOMPARE(selection.rebase(), (ArchiveCell{ 0, 0 }));
        motion.setReducedMotion(true);
        selection.advance(0);
        QCOMPARE(selection.instances().size(), 432);
    }
    void pickingUsesPublishedTransforms()
    {
        MotionController motion;
        SelectionController selection(&motion);
        selection.setColumns(columns({ 2, 3, 4 }));
        selection.setLift(4.05);
        SceneAdapter scene;
        scene.setSelection(&selection);
        scene.layout()->setViewport(960, 640);
        scene.setReducedMotion(true);
        scene.setDetail(1);
        scene.snapToTarget();
        QFile cameraGolden(QStringLiteral(FIXTURE_ROOT "/tests/fixtures/scene/camera-golden.json"));
        QVERIFY(cameraGolden.open(QIODevice::ReadOnly));
        const auto cameraCases =
                QJsonDocument::fromJson(cameraGolden.readAll()).object()["cases"].toArray();
        bool compared = false;
        for (const auto value : cameraCases) {
            const auto expected = value.toObject();
            if (expected["detail"].toDouble() != 1)
                continue;
            const auto position = expected["casePosition"].toArray();
            QVERIFY(std::abs(selection.selectedPosition().y - position[1].toDouble()) < 1e-6);
            compared = true;
            break;
        }
        QVERIFY(compared);
        const auto pixel = scene.projectWorldPoint(
                (selection.selectedPosition() + SceneVector{ 0, 1.85, 0 }).qt());
        const auto hit = scene.hitTest(pixel.x(), pixel.y());
        QCOMPARE(hit["albumId"].toString(), selection.albumId());
        QCOMPARE(hit["lane"].toDouble(), selection.cell().lane);
        QCOMPARE(hit["row"].toDouble(), selection.cell().row);
        QVERIFY(scene.hitTest(-1, -1).isEmpty());
        QVERIFY(scene.hitTest(std::numeric_limits<double>::quiet_NaN(), 1).isEmpty());
        QVERIFY(!selection.selectCell({ .5, 1 }));
        QVERIFY(!selection.moveRow(2));
    }
    void largeEmptyAndUnboundedPeriods()
    {
        MotionController motion;
        SelectionController selection(&motion);
        selection.setColumns(
                columns({ 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000 }));
        QCOMPARE(selection.instances().size(), 432);
        QVERIFY(selection.select(QStringLiteral("8:9999")));
        selection.setColumns(columns({ 97, 101, 103 }));
        QVERIFY(!std::isfinite(selection.pool().rowPeriod()));
        QVERIFY(selection.selectCell({ 0, 9999999 }));
        const auto shift = selection.rebase();
        QCOMPARE(shift.row, 0);
        selection.setColumns({ });
        QVERIFY(selection.instances().isEmpty());
        QVERIFY(!selection.moveRow(1));
    }
    void continuousEdgesAndPlaybackIsolation()
    {
        MotionController motion;
        SelectionController selection(&motion);
        auto ordered = columns({ 1, 2, 3, 7, 64, 97, 101, 113, 127 });
        selection.setColumns(ordered);
        motion.setReducedMotion(true);
        for (int direction : { 1, -1 }) {
            for (int step = 0; step < 160; ++step) {
                const auto old = selection.pool().instances();
                const auto before = selection.cell();
                QVERIFY(selection.moveRow(direction));
                selection.advance(0);
                QCOMPARE(selection.cell().row, before.row + direction);
                QCOMPARE(selection.instances().size(), 432);
                for (int i = 0; i < 432; ++i) {
                    const auto next = selection.pool().instances()[i].cell;
                    if (old[i].cell.row != next.row) {
                        QCOMPARE(std::abs(old[i].cell.row - next.row), 48);
                        // Recycling is outside the browsing camera's vertical span.
                        const auto it = std::find_if(selection.instances().cbegin(),
                                                     selection.instances().cend(),
                                                     [&](const ArchiveInstance &instance) {
                                                         return instance.cell == next;
                                                     });
                        if (it != selection.instances().cend())
                            QVERIFY(std::abs(it->transform.column(3).z() + 2.17f) > 14);
                    }
                }
            }
            for (int step = 0; step < 40; ++step) {
                const auto before = selection.cell();
                QVERIFY(selection.moveLane(direction));
                selection.advance(0);
                QCOMPARE(selection.cell().lane, before.lane + direction);
            }
        }
        OfflineMetadataProvider provider;
        LibrarySession library(&provider);
        LibrarySnapshot snapshot;
        for (int index = 0; index < 2; ++index) {
            Album album;
            album.id = QStringLiteral("album-%1").arg(index);
            album.title = QStringLiteral("Album %1").arg(index);
            Track track;
            track.id = QStringLiteral("track-%1").arg(index);
            track.albumId = album.id;
            album.tracks.append(track);
            snapshot.albums.append(album);
        }
        library.setSnapshot(std::move(snapshot));
        selection.setColumns(library.albums()->order().columns, library.snapshot());
        connect(&selection, &SelectionController::selectionChanged, &library, [&] {
            library.navigate(selection.albumId());
        });
        QVERIFY(library.setPlaybackIds("album-0", "track-0"));
        QVERIFY(selection.select("album-1"));
        library.search("Album");
        QCOMPARE(library.navigationAlbumId(), QStringLiteral("album-1"));
        QCOMPARE(library.playbackAlbumId(), QStringLiteral("album-0"));
        QCOMPARE(library.playbackTrackId(), QStringLiteral("track-0"));
        selection.setColumns(columns({ 0, 1 }));
        QCOMPARE(selection.albumId(), QStringLiteral("1:0"));
    }
};
QTEST_GUILESS_MAIN(ArchivePoolTest)
#include "ArchivePoolTest.moc"
