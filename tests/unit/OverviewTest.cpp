#include "OverviewController.h"
#include "SceneAdapter.h"
#include "DetailPorts.h"
#include "LibrarySession.h"
#include "OfflineMetadataProvider.h"
#include "PresentationController.h"
#include <QTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSignalSpy>

using namespace yalahoo;
namespace {
struct Fixture
{
    MotionController motion;
    OverviewController overview{ &motion };
    SelectionController selection{ &motion };
    SceneAdapter scene{ motion.clock() };
    OfflineMetadataProvider provider;
    LibrarySession library{ &provider };
    PresentationController presentation{ motion.clock(), detailPorts(scene, selection, library) };
    Fixture(QList<int> counts = { 1, 2, 49, 100, 3, 7, 12, 6, 4, 20000 })
    {
        QList<LibraryColumn> columns;
        for (int lane = 0; lane < counts.size(); ++lane) {
            LibraryColumn column{ QString::number(lane), QString("列 %1").arg(lane), { } };
            for (int row = 0; row < counts[lane]; ++row)
                column.albumIds.append(QString("a%1-%2").arg(lane).arg(row + 12));
            columns.append(column);
        }
        selection.setColumns(columns);
        scene.setSelection(&selection);
        overview.bind(&scene, &selection, &presentation);
        motion.clock()->setActive(true);
        motion.clock()->advance(0);
        QObject::connect(&motion, &MotionController::reducedMotionChanged, &scene, [&] {
            scene.setReducedMotion(motion.reducedMotion());
            presentation.setReducedMotion(motion.reducedMotion());
        });
    }
    void step(int count = 1)
    {
        for (int i = 0; i < count; ++i)
            motion.clock()->advance(1.0 / 120);
    }
};
QJsonObject golden()
{
    QFile file(QStringLiteral(FIXTURE_ROOT "/tests/fixtures/overview/source-golden.json"));
    if (!file.open(QIODevice::ReadOnly))
        qFatal("Missing frozen overview fixture");
    return QJsonDocument::fromJson(file.readAll()).object();
}
QVector<OverviewColumn> columns(const QJsonArray &array)
{
    QVector<OverviewColumn> result;
    for (const auto value : array) {
        const auto c = value.toObject();
        result.append({ c["lane"].toInt(), c["name"].toString(), c["count"].toInt(),
                        c["x"].toDouble(), c["y"].toDouble(), c["extent"].toDouble(),
                        c["selected"].toBool() });
    }
    return result;
}
}
class OverviewTest : public QObject
{
    Q_OBJECT
private slots:
    void frozenMotionEdgesAndCollisions()
    {
        double maximum = 0;
        const auto source = golden();
        for (const auto value : source["cases"].toArray()) {
            Fixture f({ 3, 4, 7 });
            for (const auto sample : value.toObject()["samples"].toArray()) {
                const auto s = sample.toObject();
                f.overview.setActive(s["active"].toBool());
                f.overview.advance(s["dt"].toDouble(), s["reduced"].toBool());
                maximum =
                        std::max({ maximum, std::abs(f.overview.progress() - s["value"].toDouble()),
                                   std::abs(f.overview.state().velocity - s["velocity"].toDouble()),
                                   std::abs(f.overview.state().acceleration
                                            - s["acceleration"].toDouble()) });
            }
        }
        QVERIFY2(maximum <= 1e-6, qPrintable(QString::number(maximum, 'g', 16)));
        for (const auto value : source["extents"].toArray()) {
            const auto s = value.toObject();
            QVERIFY(std::abs(OverviewController::columnExtent(s["lane"].toDouble(),
                                                              s["center"].toDouble(),
                                                              s["progress"].toDouble())
                             - s["value"].toDouble())
                    <= 1e-6);
        }
        for (const auto value : source["labels"].toArray()) {
            const auto s = value.toObject();
            QSet<int> retained;
            for (const auto lane : s["retained"].toArray())
                retained.insert(lane.toInt());
            const auto visible = OverviewLabelsModel::visibleLabels(
                    columns(s["columns"].toArray()), s["width"].toDouble(), s["height"].toDouble(),
                    retained, s["expanded"].isNull() ? -1 : s["expanded"].toInt());
            QJsonArray actual;
            for (const auto &c : visible)
                actual.append(c.lane);
            QCOMPARE(actual, s["visible"].toArray());
        }
        qInfo() << "Maximum frozen progress error" << maximum;
    }
    void frozenCameraInterpolation()
    {
        double maximum = 0;
        const auto vector = [](const QJsonValue &v) {
            const auto a = v.toArray();
            return SceneVector{ a[0].toDouble(), a[1].toDouble(), a[2].toDouble() };
        };
        for (const auto value : golden()["cameraCases"].toArray()) {
            const auto c = value.toObject(), b = c["base"].toObject(), e = c["expected"].toObject();
            const auto actual = OverviewController::framing(
                    { vector(b["position"]), vector(b["aim"]), b["span"].toDouble() },
                    c["width"].toDouble(), c["height"].toDouble(), c["progress"].toDouble());
            maximum = std::max({ maximum, (actual.position - vector(e["position"])).length(),
                                 (actual.aim - vector(e["aim"])).length(),
                                 std::abs(actual.span - e["span"].toDouble()) });
        }
        QVERIFY2(maximum <= 1e-6, qPrintable(QString::number(maximum, 'g', 16)));
        qInfo() << "Maximum frozen camera error" << maximum;
    }
    void halfLaneBoundaryKeepsSelectedPriority()
    {
        // At the wrap boundary the zero-extent endpoints have canonical IDs 4/5,
        // while selected ID 9 is last in source order. Truncating before culling loses it.
        QVector<OverviewColumn> candidates;
        for (int lane = 0; lane < 10; ++lane)
            candidates.append({ lane, QString::number(lane), 3, 180.0 + lane * 180, 300,
                                lane == 4 || lane == 5 ? 0.0 : 1.0, lane == 9 });
        const auto visible = OverviewLabelsModel::visibleLabels(candidates, 2560, 900);
        QCOMPARE(visible.size(), 8);
        QVERIFY(std::any_of(visible.cbegin(), visible.cend(), [](const auto &c) {
            return c.lane == 9 && c.selected;
        }));
    }
    void frozenProductionProjection()
    {
        Fixture f;
        QVERIFY(f.selection.selectCell({ 2, 48 }, true));
        f.motion.finish();
        double maximum = 0;
        for (const auto value : golden()["projections"].toArray()) {
            const auto s = value.toObject();
            f.selection.setRealistic(s["mode"] == "realistic");
            f.motion.finish();
            f.selection.advance(10);
            f.scene.layout()->setViewport(s["width"].toDouble(), s["height"].toDouble());
            const auto pose =
                    OverviewController::framing({ { 0, 0, 140 }, { 0, 0, 0 }, 7.33 },
                                                s["width"].toDouble(), s["height"].toDouble(), 1);
            // Projection fixture pins the wide camera independently of motion progress.
            f.scene.observe(pose, 0);
            f.overview.setActive(true);
            // Reach each exact source progress through the actual controller's rate-6.5 trajectory.
            f.overview.reset();
            f.overview.setActive(true);
            const double target = s["progress"].toDouble();
            if (target == 1)
                f.overview.advance(0, true);
            else if (target > 0) {
                double lo = 0, hi = 10;
                for (int i = 0; i < 60; ++i) {
                    const double t = (lo + hi) / 2;
                    const double p = 1 - (1 + 6.5 * t) * std::exp(-6.5 * t);
                    if (p < target)
                        lo = t;
                    else
                        hi = t;
                }
                f.overview.advance((lo + hi) / 2, false);
            }
            f.scene.observe(pose, 0);
            const auto actual = f.overview.projectColumns();
            const auto expected = columns(s["columns"].toArray());
            QCOMPARE(actual.size(), expected.size());
            for (int i = 0; i < actual.size(); ++i) {
                QCOMPARE(actual[i].lane, expected[i].lane);
                QCOMPARE(actual[i].count, expected[i].count);
                QVERIFY(std::abs(actual[i].extent - expected[i].extent) <= 1e-6);
                maximum = std::max({ maximum, std::abs(actual[i].x - expected[i].x),
                                     std::abs(actual[i].y - expected[i].y) });
            }
        }
        QVERIFY2(maximum <= 2, qPrintable(QString::number(maximum, 'g', 16)));
        qInfo() << "Maximum source projection logical-pixel error" << maximum;
    }
    void lifecycleAndTwoStepEntry()
    {
        Fixture f;
        f.scene.layout()->setViewport(1280, 720);
        const auto id = f.selection.albumId();
        f.overview.setActive(true);
        f.step(360);
        QVERIFY(f.overview.ready());
        QVERIFY(!f.presentation.browseSurface()->shown());
        QVERIFY(f.overview.labels()->liveCount() <= 18);
        QVERIFY(!f.overview.enter(0));
        int lane = -1;
        for (int row = 0; row < f.overview.labels()->rowCount(); ++row)
            if (f.overview.labels()
                        ->data(f.overview.labels()->index(row), OverviewLabelsModel::Interactive)
                        .toBool()) {
                lane = f.overview.labels()
                               ->data(f.overview.labels()->index(row), OverviewLabelsModel::Lane)
                               .toInt();
                break;
            }
        QVERIFY(lane >= 0);
        f.overview.expand(lane);
        QCOMPARE(f.selection.albumId(), id);
        QVERIFY(f.overview.labels()->canEnter(lane));
        QVERIFY(f.overview.enter(lane));
        QVERIFY(!f.overview.active());
        QVERIFY(!f.presentation.wantsDetail());
        f.step(720);
        QVERIFY(f.presentation.browseSurface()->shown());
        QCOMPARE(f.overview.labels()->liveCount(), 0);
        const auto revision = f.scene.frame().revision;
        f.motion.clock()->setActive(false);
        f.motion.clock()->advance(1);
        QCOMPARE(f.scene.frame().revision, revision);
        f.motion.clock()->setActive(true);
        f.motion.clock()->advance(1);
        QCOMPARE(f.scene.frame().revision, revision);
        f.overview.setActive(true);
        f.step(5);
        f.overview.setActive(false);
        f.step(5);
        f.overview.setActive(true);
        f.motion.setReducedMotion(true);
        QCOMPARE(f.overview.progress(), 1);
        f.overview.setActive(false);
        QCOMPARE(f.overview.progress(), 0);
        QCOMPARE(f.overview.labels()->liveCount(), 0);
        f.selection.setColumns({ });
        f.overview.setActive(true);
        QVERIFY(!f.overview.active());
    }
    void retiringAnchorReversalAndSlotReuse()
    {
        OverviewLabelsModel labels;
        OverviewColumn column{ 1, "第一列", 8, 800, 400, 1, true };
        const auto rowFor = [&](int lane) {
            for (int row = 0; row < labels.rowCount(); ++row)
                if (labels.data(labels.index(row), OverviewLabelsModel::Lane).toInt() == lane)
                    return row;
            return -1;
        };
        labels.update({ column }, 1920, 1080, true, 0, false);
        labels.update({ column }, 1920, 1080, true, .46, false);
        const int row = rowFor(1);
        QVERIFY(row >= 0);
        const auto index = labels.index(row);
        QVERIFY(labels.expand(1));
        labels.update({ column }, 1920, 1080, true, .42, false);
        QCOMPARE(labels.data(index, OverviewLabelsModel::EntryWidth).toDouble(), 76);

        labels.update({ column }, 1920, 1080, false, 0, false);
        QVERIFY(!labels.data(index, OverviewLabelsModel::Interactive).toBool());
        QVERIFY(!labels.canEnter(1));
        column.x += 37;
        column.y += 23;
        column.extent = .7;
        labels.update({ column }, 1920, 1080, false, .1, false);
        QCOMPARE(labels.data(index, OverviewLabelsModel::X).toDouble(), column.x);
        QCOMPARE(labels.data(index, OverviewLabelsModel::Y).toDouble(), column.y);
        QCOMPARE(labels.data(index, OverviewLabelsModel::Extent).toDouble(), .7);
        const double opacity = labels.data(index, OverviewLabelsModel::Fade).toDouble();
        QVERIFY(opacity > 0 && opacity < 1);
        labels.update({ column }, 1920, 1080, true, 0, false);
        QCOMPARE(rowFor(1), row);
        QCOMPARE(labels.data(index, OverviewLabelsModel::Fade).toDouble(), opacity);
        labels.update({ column }, 1920, 1080, true, 1, false);

        QVERIFY(labels.expand(1));
        labels.update({ column }, 1920, 1080, true, .42, false);
        labels.update({ column }, 1920, 1080, false, 0, false);
        // Card retirement precedes expansion collapse. Reuse must start with no
        // entry width/opacity, as a newly created source DOM card would.
        labels.update({ column }, 1920, 1080, false, .24, false);
        QCOMPARE(labels.liveCount(), 0);
        column.lane = 2;
        labels.update({ column }, 1920, 1080, true, 0, false);
        QCOMPARE(rowFor(2), row);
        QCOMPARE(labels.data(index, OverviewLabelsModel::EntryWidth).toDouble(), 0);
        QCOMPARE(labels.data(index, OverviewLabelsModel::EntryOpacity).toDouble(), 0);
        QVERIFY(!labels.canEnter(2));
        labels.update({ column }, 1920, 1080, false, 0, true);
        QCOMPARE(labels.liveCount(), 0);
    }
    void foldedColumnsCannotBePicked()
    {
        Fixture f({ 3, 3, 3, 3, 3, 3, 3, 3, 3 });
        f.scene.layout()->setViewport(1280, 720);
        f.overview.setActive(true);
        f.overview.advance(0, true);
        QVERIFY(f.selection.moveLane(1));
        bool checked = false;
        for (int frame = 0; frame < 300 && !checked; ++frame) {
            f.step();
            f.scene.observe({ { 0, 100, 70 }, { 0, -2, -2.17 }, 50 }, 0);
            for (const auto &instance : f.selection.instances()) {
                const double extent = f.selection.columnExtent(instance.cell.lane);
                if (extent <= 0 || extent > .12)
                    continue;
                const auto point =
                        f.scene.projectWorldPoint(instance.transform.map({ 0, 1.85f, 0 }));
                QVERIFY(point.x() >= 0 && point.x() < 1280 && point.y() >= 0 && point.y() < 720);
                auto isolated = f.scene.frame();
                isolated.instances = { instance };
                QVERIFY(f.selection.hitTest(isolated, point.x(), point.y()).isEmpty());
                checked = true;
                break;
            }
        }
        QVERIFY(checked);
    }
    void edgeGeometryAndFiniteCentering()
    {
        Fixture f;
        f.selection.setRealistic(true);
        f.selection.selectCell({ 2, 48 }, true);
        f.motion.finish();
        f.overview.setActive(true);
        f.overview.advance(0, true);
        f.selection.advance(0);
        QCOMPARE(f.selection.browseRow(2), 36.0);
        for (const auto &instance : f.selection.instances()) {
            QVERIFY(instance.cell.row >= 12);
            QVERIFY(instance.cell.row <= 11 + f.selection.pool().rowCount(instance.cell.lane));
        }
        // Same physical bottom is preserved; only local Y extent changes.
        f.selection.setRealistic(false);
        f.selection.selectCell({ 3, 12 });
        f.motion.finish();
        f.overview.reset();
        f.selection.advance(0);
        auto before = f.selection.instances();
        f.overview.setActive(true);
        f.overview.advance(0, true);
        f.selection.advance(0);
        for (const auto &a : before)
            for (const auto &b : f.selection.instances())
                if (a.objectId == b.objectId) {
                    const double extent = f.selection.columnExtent(a.cell.lane);
                    if (extent > 0) {
                        QVERIFY((a.transform.map({ 0, .175f, 0 })
                                 - b.transform.map({ 0, .175f, 0 }))
                                        .length()
                                < .001);
                        QVERIFY((a.transform.mapVector({ 1, 0, 0 })
                                 - b.transform.mapVector({ 1, 0, 0 }))
                                        .length()
                                < .001);
                        QVERIFY((a.transform.mapVector({ 0, 0, 1 })
                                 - b.transform.mapVector({ 0, 0, 1 }))
                                        .length()
                                < .001);
                    }
                }
    }
};
QTEST_GUILESS_MAIN(OverviewTest)
#include "OverviewTest.moc"
