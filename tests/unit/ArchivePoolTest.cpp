#include "LayoutMetrics.h"
#include "MotionController.h"
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
#include <cmath>

using namespace Qt::StringLiterals;
using namespace yalahoo;
namespace {
QList<LibraryColumn> columns(const QList<int> &counts)
{
    QList<LibraryColumn> result;
    for (int lane = 0; lane < counts.size(); ++lane) {
        LibraryColumn c;
        for (int row = 0; row < counts[lane]; ++row)
            c.albumIds.append(u"%1:%2"_s.arg(lane).arg(row));
        result.append(c);
    }
    return result;
}
ArchiveCell cell(QJsonObject value)
{
    return { value[u"lane"_s].toDouble(), value[u"row"_s].toDouble() };
}
}
class ArchivePoolTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ArchivePoolTest)
public:
    ArchivePoolTest() = default;

private:
    QJsonObject golden;
private Q_SLOTS:
    void wrappingNeverProducesTheCountAtNegativeBoundary()
    {
        const double belowZero = std::nextafter(0., -1.);
        for (const int count : { 1, 9, 48, 1000000 }) {
            QCOMPARE(ArchivePool::wrap(belowZero, count), count - 1);
            QCOMPARE(ArchivePool::wrap(-double(count), count), 0);
            QCOMPARE(ArchivePool::wrap(-1, count), count - 1);
            QCOMPARE(ArchivePool::wrap(double(count), count), 0);
        }
        ArchivePool pool;
        pool.setColumns(columns({ 1, 1, 1 }));
        QCOMPARE(pool.albumAt({ belowZero, 12 }), u"2:0"_s);
    }
    void pooledColumnLookupPreservesEveryPhysicalCell()
    {
        ArchivePool pool;
        std::array<double, ArchivePool::lanes> rowCenters;
        for (int lane = 0; lane < ArchivePool::lanes; ++lane)
            rowCenters[lane] = 12 + lane * 7.5;
        for (const QList<int> &counts : { QList<int>{ }, QList<int>{ 0 }, QList<int>{ 1, 0, 3, 97 },
                                          QList<int>{ 5, 4, 3, 2, 1, 0, 23, 51, 60, 7, 9 } }) {
            pool.setColumns(columns(counts));
            for (const ArchiveCell center :
                 { ArchiveCell{ -1000.5, -104.5 }, ArchiveCell{ 0, 12 }, ArchiveCell{ 2.5, 36 },
                   ArchiveCell{ 1000.5, 10000.5 } }) {
                for (const bool realistic : { false, true }) {
                    pool.update(center, realistic ? &rowCenters : nullptr);
                    for (int i = 0; i < ArchivePool::capacity; ++i) {
                        auto expected = ArchivePool::visibleCell(i, center);
                        if (realistic)
                            expected.row = ArchivePool::nearestOccurrence(
                                    ArchivePool::poolCell(i).row, rowCenters[i / ArchivePool::rows],
                                    ArchivePool::rows);
                        const auto &actual = pool.instances()[i];
                        QCOMPARE(actual.cell, expected);
                        QCOMPARE(actual.albumId,
                                 realistic ? pool.realAlbumAt(expected) : pool.albumAt(expected));
                        QCOMPARE(actual.objectId, quint64(i + 1));
                        QVERIFY(!actual.owned);
                    }
                }
            }
        }
    }
    void laneCommandRebasesBeforeNarrowingPeriodicCoordinates()
    {
        MotionController motion;
        SelectionController selection(&motion);
        selection.setColumns(columns({ 1, 1, 1 }));
        QVERIFY(selection.selectCell({ 1e12, 12 }));
        QCOMPARE(selection.albumId(), u"1:0"_s);
        QVERIFY(selection.moveLane(1));
        QCOMPARE(selection.albumId(), u"2:0"_s);
        QVERIFY(std::abs(selection.cell().lane) < 2048);
    }
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
        QFile file(
                (QString::fromUtf8(FIXTURE_ROOT) + u"/tests/fixtures/archive-pool/golden.json"_s));
        QVERIFY(file.open(QIODevice::ReadOnly));
        golden = QJsonDocument::fromJson(file.readAll()).object();
    }
    void frozenCellsAndDirections()
    {
        ArchivePool pool;
        QList<int> counts;
        for (auto n : golden[u"counts"_s].toArray())
            counts.append(n.toInt());
        pool.setColumns(columns(counts));
        for (auto sample : golden[u"pool"_s].toArray()) {
            const auto value = sample.toObject();
            pool.update(cell(value[u"center"_s].toObject()));
            QSet<QString> physical;
            for (const auto &instance : pool.instances())
                physical.insert(u"%1:%2"_s.arg(instance.cell.lane).arg(instance.cell.row));
            QCOMPARE(physical.size(), 432);
            for (auto expected : value[u"cells"_s].toArray()) {
                const auto e = expected.toObject();
                const auto &actual = pool.instances()[e[u"index"_s].toInt()];
                QCOMPARE(actual.cell, cell(e));
                QCOMPARE(actual.albumId, e[u"albumId"_s].toString());
            }
        }
        for (auto sample : golden[u"nearest"_s].toArray()) {
            const auto value = sample.toObject();
            const auto input = value[u"input"_s].toArray();
            QCOMPARE(ArchivePool::nearestOccurrence(input[0].toDouble(), input[1].toDouble(),
                                                    input[2].toDouble()),
                     value[u"output"_s].toDouble());
        }
        MotionController motion;
        SelectionController selection(&motion);
        selection.setColumns(columns({ 1, 2, 3, 7, 64, 97, 101, 113, 127 }));
        QVERIFY(selection.selectCell({ 2, 14 }));
        QVERIFY(selection.moveRow(1));
        QCOMPARE(selection.cell(), (ArchiveCell{ 2, 15 }));
        QCOMPARE(selection.albumId(), u"2:0"_s);
        QVERIFY(selection.moveRow(-1));
        QCOMPARE(selection.cell(), (ArchiveCell{ 2, 14 }));
        QVERIFY(selection.moveLane(1));
        QCOMPARE(selection.cell().lane, 3);
        QVERIFY(selection.select(u"1:0"_s));
        QCOMPARE(selection.cell().lane, 1);
        QCOMPARE(selection.cell().row, 12);
    }
    void frozenWaveFormulae()
    {
        for (auto sample : golden[u"fields"_s].toArray()) {
            const auto v = sample.toObject();
            const double t = v[u"time"_s].toDouble(), r = v[u"row"_s].toDouble(),
                         l = v[u"lane"_s].toDouble();
            QVERIFY(std::abs(archiveWave(r, l, t) - v[u"archive"_s].toDouble()) < 1e-10);
            QVERIFY(std::abs(idleWave(r, l, t) - v[u"idle"_s].toDouble()) < 1e-10);
            QVERIFY(std::abs(musicSelectionWave(std::abs(r - 12), t) - v[u"pulse"_s].toDouble())
                    < 1e-10);
            QVERIFY(std::abs(settlingWave(r - 12, 26.56) * columnStrength(l, 2)
                             - v[u"resting"_s].toDouble())
                    < 1e-10);
            QVERIFY(std::abs(extraction(t) - v[u"extraction"_s].toDouble()) < 1e-10);
            QVERIFY(std::abs(returnStep(.8, t) - v[u"returnAngle"_s].toDouble()) < 1e-10);
        }
    }
    void pulseEnvelopeRefreshesWithSceneTime()
    {
        for (const bool realistic : { false, true }) {
            MotionController motion;
            SelectionController selection(&motion);
            selection.setColumns(columns({ 5 }));
            selection.setRealistic(realistic);
            QVERIFY(selection.selectCell({ 0, 13 }));
            double age = 0;
            for (const double dt : { 0., .05, .2, .46, .3 }) {
                age += dt;
                selection.advance(dt);
                const double center = realistic ? selection.realColumns().row(0)
                                                : motion.springs()[ShoulderAxis].value;
                const double strength =
                        realistic ? 1 : columnStrength(0, motion.springs()[LaneAxis].value);
                for (const double row : { 12., 13., 13.5, 16. }) {
                    const double expected = settlingWave(row - center, 26.56) * strength
                            + musicSelectionWave(std::abs(row - 13), age);
                    QVERIFY(std::abs(selection.field(row, 0) - expected) < 1e-10);
                }
            }
            selection.setColumns(columns({ 5 }));
            const double center = realistic ? selection.realColumns().row(0)
                                            : motion.springs()[ShoulderAxis].value;
            QCOMPARE(selection.field(13, 0), settlingWave(13 - center, 26.56));
        }
    }
    void frozenProductionTransforms()
    {
        MotionController motion;
        SelectionController selection(&motion);
        selection.setColumns(columns({ 1, 2, 3, 7, 64, 97, 101, 113, 127 }));
        for (auto sample : golden[u"poses"_s].toArray()) {
            const auto value = sample.toObject();
            const auto targetCell = cell(value);
            const auto it =
                    std::find_if(selection.instances().cbegin(), selection.instances().cend(),
                                 [&](const ArchiveInstance &i) {
                                     return i.cell == targetCell;
                                 });
            QVERIFY(it != selection.instances().cend());
            QCOMPARE(it->albumId, value[u"albumId"_s].toString());
            const auto expected = value[u"matrix"_s].toArray();
            for (int i = 0; i < 16; ++i)
                QVERIFY(std::abs(it->transform.constData()[i] - expected[i].toDouble()) < 1e-5);
        }
        for (auto sample : golden[u"ownedPoses"_s].toArray()) {
            const auto value = sample.toObject();
            auto springs = motion.springs();
            springs[2].value = value[u"shoulder"_s].toDouble();
            motion.observe(
                    springs,
                    { springs[0].value, springs[1].value, springs[2].value, springs[3].value },
                    { 3.7, 3.7, 5, 4 });
            selection.setLift(value[u"lift"_s].toDouble());
            selection.setRotation(value[u"rotation"_s].toDouble());
            const auto expected = value[u"matrix"_s].toArray();
            const auto &matrix = selection.instances().last().transform;
            for (int i = 0; i < 16; ++i)
                QVERIFY(std::abs(matrix.constData()[i] - expected[i].toDouble()) < 1e-5);
        }
        for (auto sample : golden[u"selection"_s].toArray()) {
            const auto value = sample.toObject();
            selection.setColumns(columns({ 1, 2, 3, 7, 64, 97, 101, 113, 127 }));
            QVERIFY(selection.selectCell(cell(value[u"current"_s].toObject())));
            selection.setLift(0);
            const auto navigation = value[u"navigation"_s].toObject();
            if (navigation[u"axis"_s] == u"row"_s)
                QVERIFY(selection.moveRow(navigation[u"direction"_s].toInt()));
            else if (navigation[u"axis"_s] == u"lane"_s)
                QVERIFY(selection.selectCell(
                        { selection.cell().lane + navigation[u"direction"_s].toDouble(),
                          std::round(navigation[u"row"_s].toDouble()) }));
            else
                QVERIFY(selection.select(value[u"id"_s].toString()));
            QCOMPARE(selection.cell(), cell(value[u"cell"_s].toObject()));
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
            const auto key = u"%1:%2"_s.arg(i.cell.lane).arg(i.cell.row);
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
    void physicalObjectIdsSurviveOverlappingFrontiers()
    {
        MotionController motion;
        SelectionController selection(&motion);
        selection.setColumns(columns({ 2, 3, 4 }));
        selection.setLift(0);
        QHash<QPair<qint64, qint64>, quint64> previous;
        const auto verify = [&] {
            QHash<QPair<qint64, qint64>, quint64> current;
            QSet<quint64> identities;
            const auto origin = selection.coordinateOrigin();
            for (const auto &instance : selection.instances()) {
                QVERIFY(!identities.contains(instance.objectId));
                identities.insert(instance.objectId);
                if (instance.owned)
                    continue;
                const auto physical = qMakePair(qint64(instance.cell.lane + origin.lane),
                                                qint64(instance.cell.row + origin.row));
                const auto before = previous.constFind(physical);
                if (before != previous.cend())
                    QCOMPARE(instance.objectId, *before);
                current.insert(physical, instance.objectId);
            }
            previous = std::move(current);
        };
        verify();
        for (const ArchiveCell target : { ArchiveCell{ 1, 13 },
                                          { -1, -2 },
                                          { 0, -1 },
                                          { 8, 46 },
                                          { 9, 47 },
                                          { -18, -49 },
                                          { -17, -48 },
                                          { 3002, 4097 } }) {
            QVERIFY(selection.selectCell(target));
            motion.finish();
            selection.setLift(0);
            verify();
        }
        const auto before = previous;
        const auto shift = selection.rebase();
        QVERIFY(shift.lane != 0 && shift.row != 0);
        verify();
        QCOMPARE(previous, before);
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
        QFile cameraGolden(
                (QString::fromUtf8(FIXTURE_ROOT) + u"/tests/fixtures/scene/camera-golden.json"_s));
        QVERIFY(cameraGolden.open(QIODevice::ReadOnly));
        const auto cameraCases =
                QJsonDocument::fromJson(cameraGolden.readAll()).object()[u"cases"_s].toArray();
        bool compared = false;
        for (const auto value : cameraCases) {
            const auto expected = value.toObject();
            if (expected[u"detail"_s].toDouble() != 1)
                continue;
            const auto position = expected[u"casePosition"_s].toArray();
            QVERIFY(std::abs(selection.selectedPosition().y - position[1].toDouble()) < 1e-6);
            compared = true;
            break;
        }
        QVERIFY(compared);
        const auto pixel = scene.projectWorldPoint(
                (selection.selectedPosition() + SceneVector{ 0, 1.85, 0 }).qt());
        QVERIFY(pixel);
        const auto hit = scene.hitTest(pixel->x(), pixel->y());
        QCOMPARE(hit[u"albumId"_s].toString(), selection.albumId());
        QCOMPARE(hit[u"lane"_s].toDouble(), selection.cell().lane);
        QCOMPARE(hit[u"row"_s].toDouble(), selection.cell().row);
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
        QVERIFY(selection.select(u"8:9999"_s));
        selection.setColumns(columns({ 97, 101, 103 }));
        QVERIFY(!selection.pool().rowPeriod());
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
            album.id = u"album-%1"_s.arg(index);
            album.title = u"Album %1"_s.arg(index);
            Track track;
            track.id = u"track-%1"_s.arg(index);
            track.albumId = album.id;
            album.tracks.append(track);
            snapshot.albums.append(album);
        }
        library.setSnapshot(std::move(snapshot));
        selection.setColumns(library.albums()->order().columns, library.snapshot());
        connect(&selection, &SelectionController::selectionChanged, &library, [&] {
            library.navigate(selection.albumId());
        });
        QVERIFY(library.setPlaybackIds(u"album-0"_s, u"track-0"_s));
        QVERIFY(selection.select(u"album-1"_s));
        library.search(u"Album"_s);
        QCOMPARE(library.navigationAlbumId(), u"album-1"_s);
        QCOMPARE(library.playbackAlbumId(), u"album-0"_s);
        QCOMPARE(library.playbackTrackId(), u"track-0"_s);
        selection.setColumns(columns({ 0, 1 }));
        QCOMPARE(selection.albumId(), u"1:0"_s);
    }
};
QTEST_GUILESS_MAIN(ArchivePoolTest)
#include "ArchivePoolTest.moc"
