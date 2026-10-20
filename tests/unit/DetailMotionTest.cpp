// Checks extraction, interrupted exchange and return-copy state independently of rendered
// images. Explicit fixture literals are independent inputs or expected results; timing bounds
// have named constants.

#include "PresentationController.h"
#include "MotionController.h"
#include "SelectionController.h"
#include "DetailPorts.h"
#include "DetailModel.h"
#include "SceneAdapter.h"
#include "LibrarySession.h"
#include "OfflineMetadataProvider.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSignalSpy>
#include <QTest>
#include <cmath>

using namespace Qt::StringLiterals;

using namespace yalahoo;
namespace {
struct Fixture final
{
    MotionController motion;
    SelectionController selection{ &motion };
    SceneAdapter scene{ motion.clock() };
    OfflineMetadataProvider provider;
    LibrarySession library{ &provider };
    PresentationController presentation{ motion.clock(), detailPorts(scene, selection, library) };
    Fixture()
    {
        LibrarySnapshot data;
        LibraryColumn column;
        column.key = "g"_L1;
        for (int row = 0; row < 12; ++row) {
            Album album;
            album.id = u"a%1"_s.arg(row);
            album.title = album.id;
            data.albums.append(album);
            column.albumIds.append(album.id);
        }
        library.setSnapshot(std::move(data));
        selection.setColumns({ column }, library.snapshot());
        scene.setSelection(&selection);
        library.navigate(selection.albumId());
        QObject::connect(&presentation, &PresentationController::prepared, &presentation, [&] {
            presentation.layoutReady(presentation.layoutRevision());
        });
        QObject::connect(&presentation, &PresentationController::changed, &selection, [&] {
            if (!presentation.detailEnabled())
                selection.cancelDrag();
        });
        QObject::connect(&motion, &MotionController::reducedMotionChanged, &scene, [&] {
            scene.setReducedMotion(motion.reducedMotion());
            presentation.setReducedMotion(motion.reducedMotion());
        });
        motion.clock()->setActive(true);
        motion.clock()->advance(0);
    }
    void step(int frames = 1)
    {
        for (int i = 0; i < frames; ++i)
            motion.clock()->advance(1.0 / 120);
    }
    bool until(const QString &phase)
    {
        for (int i = 0; i < 2400; ++i) {
            if (presentation.phase() == phase)
                return true;
            step();
        }
        return false;
    }
    void open()
    {
        presentation.open();
        step(480);
    }
};
QString phaseName(DetailMotion::Phase phase)
{
    switch (phase) {
    case DetailMotion::Phase::Archive:
        return u"archive"_s;
    case DetailMotion::Phase::Placing:
        return u"placing"_s;
    case DetailMotion::Phase::Presented:
        return u"presented"_s;
    case DetailMotion::Phase::ReturningCenter:
        return u"returning-center"_s;
    case DetailMotion::Phase::ReturningArray:
        return u"returning-array"_s;
    }
    return { };
}
}
class DetailMotionTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DetailMotionTest)
public:
    DetailMotionTest() = default;

private Q_SLOTS:
    void frozenPlacementAndRotation()
    {
        QFile file(
                (QString::fromUtf8(FIXTURE_ROOT) + u"/tests/fixtures/motion/detail-golden.json"_s));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto golden = QJsonDocument::fromJson(file.readAll()).object();
        double errors[3] = { };
        for (const auto value : golden[u"cases"_s].toArray()) {
            DetailMotion detail;
            for (const auto sample : value.toObject()[u"samples"_s].toArray()) {
                const auto s = sample.toObject();
                for (const auto event : s[u"events"_s].toArray()) {
                    if (event == u"open"_s)
                        detail.request(true);
                    if (event == u"return"_s)
                        detail.request(false);
                    if (event == u"selection"_s)
                        detail.selectionChanged();
                }
                const auto dt = s[u"dt"_s].toDouble();
                const bool reduced = s[u"reduced"_s].toBool();
                detail.advance(dt, reduced, true);
                detail.updateReadiness(dt, detail.settled(), detail.settled(), reduced);
                QCOMPARE(phaseName(detail.phase()), s[u"phase"_s].toString());
                const auto expected = s[u"state"_s].toArray();
                const auto state = detail.state();
                const double actual[] = { state.value, state.velocity, state.acceleration };
                for (int i = 0; i < 3; ++i)
                    errors[i] = std::max(errors[i], std::abs(actual[i] - expected[i].toDouble()));
            }
        }
        for (double error : errors)
            QVERIFY2(error <= 1e-6, qPrintable(QString::number(error, 'g', 16)));
        qInfo() << "Frozen placement max x/v/a error:" << errors[0] << errors[1] << errors[2];
        for (const bool reduced : { false, true }) {
            PointerDragController drag;
            double rotation = 0;
            for (int frame = 0; frame < 360; ++frame) {
                if (frame == 0 || frame == 60) {
                    drag.press({ 0, 0 });
                    drag.move({ frame == 0 ? 200.0 : -400.0, 0 });
                    drag.release({ 0, 0 });
                }
                rotation = drag.advance(rotation, 1.0 / 120, frame < 120, reduced);
                for (const auto value : golden[u"rotations"_s].toArray()) {
                    const auto s = value.toObject();
                    if (s[u"frame"_s].toInt() == frame && s[u"reduced"_s].toBool() == reduced)
                        QVERIFY(std::abs(rotation - s[u"rotation"_s].toDouble()) <= 1e-6);
                }
            }
        }
    }
    void frozenClarityAndIndependentReturningMaterial()
    {
        QFile file(
                (QString::fromUtf8(FIXTURE_ROOT) + u"/tests/fixtures/motion/detail-golden.json"_s));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto samples =
                QJsonDocument::fromJson(file.readAll()).object()[u"claritySamples"_s].toArray();
        QCOMPARE(samples.size(), 1200);
        CaseClarity clarity;
        double error = 0;
        for (const auto &value : samples) {
            const auto sample = value.toArray();
            const auto event = sample[0].toString();
            if (event == "enter"_L1)
                clarity.enter();
            if (event == "leave"_L1)
                clarity.leave();
            if (event == "reclaim"_L1) {
                clarity.select(.65);
                clarity.enter();
            }
            if (event == "already-clear"_L1) {
                clarity.select(1);
                clarity.enter(true);
            }
            if (event == "new"_L1) {
                clarity.select();
                clarity.enter();
            }
            clarity.advance(sample[1].toDouble(), sample[2].toBool(), sample[3].toBool());
            error = std::max(error, std::abs(clarity.value() - sample[4].toDouble()));
        }
        QVERIFY(error <= 1e-6);
        qInfo() << "Frozen material clarity max error:" << error;
        Fixture f;
        f.open();
        f.step(480);
        QCOMPARE(f.selection.selectedClarity(), 1);
        QVERIFY(f.scene.frame().individualMaterials);
        QVERIFY(f.selection.switchDetail(u"a1"_s));
        QCOMPARE(f.selection.selectedClarity(), 0);
        QCOMPARE(f.selection.returningCases().begin()->clarity, 1);
        f.step();
        const double returning = f.selection.returningCases().begin()->clarity;
        QVERIFY(std::abs(returning - std::exp(-9.0 / 120)) < 1e-12);
        for (const auto &instance : f.selection.instances())
            if (!instance.owned)
                QCOMPARE(instance.clarity, 0);
        QVERIFY(f.selection.switchDetail(u"a0"_s));
        QCOMPARE(f.selection.selectedClarity(), returning);
        f.motion.setReducedMotion(true);
        f.step();
        QCOMPARE(f.selection.selectedClarity(), 1);
        QCOMPARE(f.selection.returningCases().size(), 0);
    }
    void simultaneousExtractionAndStableSwitchCamera()
    {
        Fixture f;
        const auto camera = f.scene.cameraPosition();
        f.presentation.open();
        QVERIFY(f.until(u"opening"_s));
        // Browse must finish before the extraction track starts.
        QCOMPARE(f.selection.detailMotion().value(), 0);
        f.step(40);
        QVERIFY(f.selection.detailMotion().value() > 0);
        QVERIFY(f.selection.liftState().value > .9);
        QVERIFY(f.scene.cameraPosition() != camera);
        QCOMPARE(f.selection.liftState().value, .9 + 2.57 * f.selection.detailMotion().value());
        QVERIFY(f.until(u"detail"_s));
        f.step(240);
        const auto fixed = f.scene.target();
        for (const auto &id : { u"a1"_s, u"a2"_s, u"a3"_s, u"a2"_s }) {
            f.presentation.select(id, true);
            QVERIFY(f.until(u"switching"_s));
            QCOMPARE(f.scene.detail(), 1);
            QCOMPARE(f.scene.target().position, fixed.position);
            QCOMPARE(f.scene.target().aim, fixed.aim);
            QVERIFY(!f.motion.moving());
            QVERIFY(f.until(u"detail"_s));
            QCOMPARE(f.selection.albumId(), id);
            f.step(240);
        }
        QCOMPARE(f.library.playbackTrackId(), QString());
        QVERIFY(f.selection.returningCases().size() <= 3);
    }
    void reverseReclaimsPhysicalOwnerAndCapacityIsAtomic()
    {
        Fixture f;
        f.open();
        QCOMPARE(f.presentation.phase(), u"detail"_s);
        const auto original = f.selection.instances().last();
        QVERIFY(f.selection.switchDetail(u"a1"_s));
        const auto copy = f.selection.returningCases().begin();
        QCOMPARE(copy->albumId, u"a0"_s);
        QCOMPARE(copy->objectId, original.objectId);
        QVERIFY(f.selection.switchDetail(u"a0"_s));
        QCOMPARE(f.selection.instances().last().objectId, original.objectId);
        QCOMPARE(f.selection.instances().last().transform, original.transform);
        for (const auto &id : { u"a1"_s, u"a2"_s, u"a3"_s }) {
            f.selection.setLift(3.47);
            QVERIFY(f.selection.switchDetail(id));
        }
        QCOMPARE(f.selection.returningCases().size(), 3);
        f.selection.setLift(3.47);
        const auto selected = f.selection.instances().last();
        QVERIFY(!f.selection.switchDetail(u"a4"_s));
        QCOMPARE(f.selection.instances().last(), selected);
        QCOMPARE(f.selection.returningCases().size(), 3);
        // Reclaim at full capacity is still allowed, with no eviction or cover swap.
        const auto back = *f.selection.returningCases().begin();
        QVERIFY(f.selection.switchDetail(back.albumId));
        QCOMPARE(f.selection.instances().last().objectId, back.objectId);
        QCOMPARE(f.selection.returningCases().size(), 3);
    }
    void dragAlignmentHoldsAltitudeAndNeighborWaits()
    {
        Fixture f;
        f.open();
        QVERIFY(f.selection.canInspect());
        QVERIFY(f.selection.beginDrag({ 100, 100 }));
        QVERIFY(f.selection.moveDrag({ 400, 600 }));
        f.step(120);
        QVERIFY(f.selection.rotation() > .7 && f.selection.rotation() <= .8);
        const auto old = f.selection.instances().last();
        const double y = f.selection.selectedPosition().y;
        f.presentation.select(u"a1"_s, true);
        QVERIFY(!f.selection.dragging());
        QVERIFY(f.until(u"switching"_s));
        QVERIFY(!f.selection.returningCases().begin()->returnY.has_value()
                || std::abs(f.selection.instances()[f.selection.instances().size() - 2].transform(1,
                                                                                                  3)
                            - y)
                        < 1e-5);
        const auto copy = *f.selection.returningCases().begin();
        QCOMPARE(copy.objectId, old.objectId);
        QCOMPARE(copy.albumId, old.albumId);
        f.step(1);
        if (f.selection.returningCases().begin()->returnY)
            QCOMPARE(f.selection.liftState().value, 0);
        QVERIFY(f.until(u"detail"_s));
        f.step(240);
        QVERIFY(f.selection.beginDrag({ 100, 100 }));
        f.selection.moveDrag({ -100, 100 });
        f.step(60);
        f.presentation.back();
        QVERIFY(f.until(u"returning"_s));
        const auto heldY = f.selection.selectedPosition().y;
        f.step();
        if (f.selection.rotation() != 0) {
            QCOMPARE(f.selection.detailMotion().phase(), DetailMotion::Phase::ReturningCenter);
            QCOMPARE(f.scene.detail(), 1);
            QVERIFY(std::abs(f.selection.selectedPosition().y - heldY) < 1e-5);
        }
        QVERIFY(f.until(u"archive"_s));
        f.step(480);
        QVERIFY(f.selection.archiveReady());
    }
    void backSearchReducedAndHiddenResume()
    {
        Fixture f;
        f.open();
        f.presentation.select(u"a1"_s, true);
        QVERIFY(f.until(u"switching"_s));
        f.step(25);
        const auto lift = f.selection.liftState();
        f.presentation.back();
        QVERIFY(std::abs(f.selection.liftState().value - lift.value) < 1e-12);
        f.presentation.select(u"a8"_s, true, true, u"track-intent"_s);
        f.presentation.select(u"a7"_s, true, true);
        QVERIFY(f.until(u"detail"_s));
        QCOMPARE(f.selection.albumId(), u"a7"_s);
        QCOMPARE(f.library.playbackTrackId(), QString());
        f.presentation.select(u"a6"_s, true);
        f.step(20);
        const auto state = f.selection.liftState();
        f.motion.clock()->setActive(false);
        f.step(120);
        QCOMPARE(f.selection.liftState().value, state.value);
        f.motion.clock()->setActive(true);
        f.step();
        QCOMPARE(f.selection.liftState().value, state.value);
        f.motion.setReducedMotion(true);
        f.step(5);
        QCOMPARE(f.presentation.phase(), u"detail"_s);
        QCOMPARE(f.scene.detail(), 1);
        QCOMPARE(f.selection.liftState().value, 3.47);
        QCOMPARE(f.selection.returningCases().size(), 0);
        f.presentation.back();
        f.step(5);
        QCOMPARE(f.presentation.phase(), u"archive"_s);
        QCOMPARE(f.scene.detail(), 0);
    }
    void reducedRotatedSnapshotReturnCompletesImmediately()
    {
        Fixture f;
        f.scene.setReducedMotion(true);
        f.scene.setDetail(1);
        f.selection.setLift(4.05);
        f.motion.setReducedMotion(true);
        f.selection.advance(0);
        f.motion.setReducedMotion(false);
        f.selection.setRotation(.6);
        QVERIFY(f.selection.moveRow(1));
        f.selection.setLift(4.05);
        QVERIFY(f.selection.moveRow(1));
        f.selection.setLift(4.05);
        QVERIFY(f.selection.moveRow(1));
        f.selection.setLift(4.05);
        QCOMPARE(f.selection.returningCases().size(), 3);
        f.motion.setReducedMotion(true);
        QCOMPARE(f.selection.returningCases().size(), 0);
    }
    void responsiveDragUsesEntityTransformAndRealColumnBoundaries()
    {
        const QList<QSize> sizes{ { 1920, 1080 }, { 1280, 720 },  { 995, 858 },
                                  { 960, 900 },   { 930, 900 },   { 390, 844 },
                                  { 320, 720 },   { 2560, 1080 }, { 3440, 1440 } };
        for (const auto &size : sizes) {
            Fixture f;
            f.scene.layout()->setViewport(size.width(), size.height());
            f.selection.setRealistic(true);
            f.step(360);
            f.open();
            QVERIFY(f.until(u"detail"_s));
            QVERIFY(!f.presentation.canMovePrevious());
            QVERIFY(f.presentation.canMoveNext());
            const auto revision = f.presentation.layoutRevision();
            f.presentation.stepAlbum(-1);
            QCOMPARE(f.presentation.layoutRevision(), revision);
            const auto fixed = f.scene.target();
            QVERIFY(f.selection.beginDrag({ 0, 0 }));
            f.selection.moveDrag({ -200, 90 });
            f.step(180);
            const auto transform = f.selection.instances().last().transform;
            const auto world = transform.map(QVector3D(0, 1.85, 0));
            const auto projected = f.scene.projectWorldPoint(world);
            const auto hit = f.scene.hitTest(projected.x(), projected.y());
            QCOMPARE(hit[u"albumId"_s].toString(), f.selection.albumId());
            QCOMPARE(hit[u"objectId"_s].toULongLong(), f.selection.instances().last().objectId);
            QVERIFY(std::abs(transform(0, 2) - std::sin(f.selection.rotation())) < 1e-6);
            f.presentation.stepAlbum(1);
            QVERIFY(f.until(u"detail"_s));
            QCOMPARE(f.scene.target().position, fixed.position);
            QCOMPARE(f.selection.albumId(), u"a1"_s);
        }
    }
    void singleFilledColumnKeepsSourceButtonsWithoutRestarting()
    {
        Fixture f;
        LibraryColumn column;
        column.key = "single"_L1;
        column.albumIds = { u"a0"_s };
        f.selection.setColumns({ column }, f.library.snapshot());
        f.open();
        QCOMPARE(f.presentation.phase(), u"detail"_s);
        QVERIFY(f.presentation.canMovePrevious());
        QVERIFY(f.presentation.canMoveNext());
        const auto revision = f.presentation.layoutRevision();
        f.presentation.stepAlbum(1);
        QCOMPARE(f.presentation.phase(), u"detail"_s);
        QCOMPARE(f.presentation.layoutRevision(), revision);
        f.selection.setRealistic(true);
        QVERIFY(!f.presentation.canMovePrevious());
        QVERIFY(!f.presentation.canMoveNext());
        f.selection.setColumns({ });
        QVERIFY(!f.presentation.canMovePrevious());
        QVERIFY(!f.presentation.canMoveNext());
    }
    void replacementInvalidatesAllReturningAndDragState()
    {
        Fixture f;
        f.open();
        f.selection.switchDetail(u"a1"_s);
        f.step(40);
        QVERIFY(f.selection.returningCases().size() > 0);
        f.selection.setColumns({ });
        QVERIFY(f.selection.instances().isEmpty());
        QCOMPARE(f.selection.returningCases().size(), 0);
        QCOMPARE(f.selection.detailMotion().phase(), DetailMotion::Phase::Archive);
        QVERIFY(!f.selection.dragging());
        QCOMPARE(f.scene.detail(), 0);
    }
    void boundedPendingAndClickDragCancellation()
    {
        Fixture f;
        f.open();
        for (const auto &id : { u"a1"_s, u"a2"_s, u"a3"_s }) {
            f.selection.setLift(3.47);
            QVERIFY(f.selection.switchDetail(id));
        }
        f.selection.setLift(3.47);
        f.presentation.select(u"a4"_s, true);
        f.presentation.detailSurface()->finish();
        QCOMPARE(f.selection.albumId(), u"a3"_s);
        QVERIFY(f.presentation.pendingSelection().has_value());
        f.presentation.select(u"a5"_s, true);
        QVERIFY(f.until(u"detail"_s));
        QCOMPARE(f.selection.albumId(), u"a5"_s);
        f.step(300);
        QCOMPARE(f.selection.returningCases().size(), 0);
        PointerDragController drag;
        QVERIFY(drag.press({ 0, 0 }, 7));
        QVERIFY(!drag.move({ 400, 0 }, 8));
        QVERIFY(drag.move({ 0, 400 }, 7));
        QCOMPARE(drag.target(), 0);
        QVERIFY(!drag.release({ 0, 400 }, 7));
        QVERIFY(drag.press({ 0, 0 }, 7));
        QVERIFY(!drag.press({ 0, 0 }, 8));
        QVERIFY(!drag.dragging());
        QVERIFY(drag.press({ 0, 0 }));
        QVERIFY(drag.release({ 2, 3 }));
    }
};
QTEST_GUILESS_MAIN(DetailMotionTest)
#include "DetailMotionTest.moc"
