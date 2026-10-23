// Checks analytic camera framing and bounded label admission against independent expected source
// values. Explicit fixture literals are independent inputs or expected results; timing bounds
// have named constants.

#include "NumericChecks.h"
#include "LayoutMetrics.h"
#include "MotionController.h"
#include "SelectionController.h"
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
#include <memory>
#include <limits>

using namespace Qt::StringLiterals;

using namespace yalahoo;
using yalahoo::test::errorMagnitude;
namespace {
struct Fixture final
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
            LibraryColumn column{ QString::number(lane), u"\u5217 %1"_s.arg(lane), { } };
            for (int row = 0; row < counts[lane]; ++row)
                column.albumIds.append(u"a%1-%2"_s.arg(lane).arg(row + 12));
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
    QFile file(
            (QString::fromUtf8(FIXTURE_ROOT) + u"/tests/fixtures/overview/source-golden.json"_s));
    if (!file.open(QIODevice::ReadOnly))
        qFatal("Missing frozen overview fixture");
    return QJsonDocument::fromJson(file.readAll()).object();
}
QVector<OverviewColumn> columns(const QJsonArray &array)
{
    QVector<OverviewColumn> result;
    for (const auto value : array) {
        const auto c = value.toObject();
        result.append({ c[u"lane"_s].toInt(), c[u"name"_s].toString(), c[u"count"_s].toInt(),
                        c[u"x"_s].toDouble(), c[u"y"_s].toDouble(), c[u"extent"_s].toDouble(),
                        c[u"selected"_s].toBool() });
    }
    return result;
}
}
class OverviewTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(OverviewTest)
public:
    OverviewTest() = default;

private Q_SLOTS:
    void labelNotificationsFollowChangedRoles()
    {
        OverviewLabelsModel model;
        QVector<OverviewColumn> input{ { 0, u"Column"_s, 12, 960, 350, 1, true } };
        model.update(input, 1920, 1080, true, 0, true);
        QSignalSpy changes(&model, &QAbstractItemModel::dataChanged);
        model.update(input, 1920, 1080, true, .05, true);
        QVERIFY(changes.isEmpty());
        input[0].x += 1;
        model.update(input, 1920, 1080, true, .05, true);
        QCOMPARE(changes.size(), 1);
        QCOMPARE(qvariant_cast<QList<int>>(changes.takeFirst()[2]),
                 QList<int>{ OverviewLabelsModel::X });
        QVERIFY(model.expand(0));
        model.update(input, 1920, 1080, true, .05, true);
        QCOMPARE(changes.size(), 1);
        const auto expanded = qvariant_cast<QList<int>>(changes.takeFirst()[2]);
        QVERIFY(expanded.contains(OverviewLabelsModel::Expanded));
        QVERIFY(expanded.contains(OverviewLabelsModel::EntryWidth));
        QVERIFY(!expanded.contains(OverviewLabelsModel::Name));
        model.update(input, 1920, 1080, true, .05, true);
        QVERIFY(changes.isEmpty());
        model.collapse();
        model.update(input, 1920, 1080, true, .05, true);
        QCOMPARE(changes.size(), 1);
        QVERIFY(qvariant_cast<QList<int>>(changes.takeFirst()[2])
                        .contains(OverviewLabelsModel::Expanded));
        model.clear();
        QVERIFY(!changes.isEmpty());
        changes.clear();
        model.clear();
        QVERIFY(changes.isEmpty());
    }
    void dependencyTeardownClearsOverviewAndRejectsNewCommands()
    {
        for (int removed = 0; removed < 4; ++removed) {
            auto motion = std::make_unique<MotionController>();
            auto selection = std::make_unique<SelectionController>(motion.get());
            auto scene = std::make_unique<SceneAdapter>(motion->clock());
            auto presentation =
                    std::make_unique<PresentationController>(motion->clock(), PresentationPorts{ });
            OverviewController overview(motion.get());
            selection->setColumns({ LibraryColumn{ u"one"_s, u"One"_s, { u"album"_s } } });
            scene->setSelection(selection.get());
            overview.bind(scene.get(), selection.get(), presentation.get());
            overview.setActive(true);
            overview.advance(1, true);
            overview.synchronize(0, true);
            QVERIFY(overview.active());
            if (removed == 0)
                scene.reset();
            else if (removed == 1)
                selection.reset();
            else if (removed == 2)
                presentation.reset();
            else
                motion.reset();
            QVERIFY(!overview.active());
            QCOMPARE(overview.labels()->liveCount(), 0);
            if (motion)
                motion->setReducedMotion(true);
            overview.toggle();
            overview.advance(.1, true);
            overview.synchronize(.1, true);
            overview.expand(0);
            QVERIFY(!overview.enter(0));
            QVERIFY(!overview.active());
        }
    }
    void maximumErrorRejectsNonfiniteComponents()
    {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        for (const double invalid : { nan, std::numeric_limits<double>::infinity(),
                                      -std::numeric_limits<double>::infinity() }) {
            QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                     static_cast<void>(std::max(
                                             { 0.0, errorMagnitude(invalid), errorMagnitude(0) })));
            const SceneVector expected{ 0, 0, 0 };
            for (const SceneVector actual :
                 { SceneVector{ invalid, 0, 0 }, SceneVector{ 0, invalid, 0 },
                   SceneVector{ 0, 0, invalid } })
                QVERIFY_THROWS_EXCEPTION(
                        std::runtime_error,
                        static_cast<void>(
                                std::max(0.0, errorMagnitude((actual - expected).length()))));
        }
    }
    void frozenMotionEdgesAndCollisions()
    {
        double maximum = 0;
        const auto source = golden();
        for (const auto value : source[u"cases"_s].toArray()) {
            Fixture f({ 3, 4, 7 });
            for (const auto sample : value.toObject()[u"samples"_s].toArray()) {
                const auto s = sample.toObject();
                f.overview.setActive(s[u"active"_s].toBool());
                f.overview.advance(s[u"dt"_s].toDouble(), s[u"reduced"_s].toBool());
                maximum = std::max(
                        { maximum, errorMagnitude(f.overview.progress() - s[u"value"_s].toDouble()),
                          errorMagnitude(f.overview.state().velocity - s[u"velocity"_s].toDouble()),
                          errorMagnitude(f.overview.state().acceleration
                                         - s[u"acceleration"_s].toDouble()) });
            }
        }
        QVERIFY2(maximum <= 1e-6, qPrintable(QString::number(maximum, 'g', 16)));
        for (const auto value : source[u"extents"_s].toArray()) {
            const auto s = value.toObject();
            QVERIFY(errorMagnitude(OverviewController::columnExtent(s[u"lane"_s].toDouble(),
                                                                    s[u"center"_s].toDouble(),
                                                                    s[u"progress"_s].toDouble())
                                   - s[u"value"_s].toDouble())
                    <= 1e-6);
        }
        for (const auto value : source[u"labels"_s].toArray()) {
            const auto s = value.toObject();
            QList<int> retained;
            for (const auto lane : s[u"retained"_s].toArray())
                retained.append(lane.toInt());
            const auto visible = OverviewLabelsModel::visibleLabels(
                    columns(s[u"columns"_s].toArray()), s[u"width"_s].toDouble(),
                    s[u"height"_s].toDouble(), std::span(retained.constData(), retained.size()),
                    s[u"expanded"_s].isNull() ? -1 : s[u"expanded"_s].toInt());
            QJsonArray actual;
            for (const auto &c : visible)
                actual.append(c.lane);
            QCOMPARE(actual, s[u"visible"_s].toArray());
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
        for (const auto value : golden()[u"cameraCases"_s].toArray()) {
            const auto c = value.toObject(), b = c[u"base"_s].toObject(),
                       e = c[u"expected"_s].toObject();
            const auto actual = OverviewController::framing(
                    { vector(b[u"position"_s]), vector(b[u"aim"_s]), b[u"span"_s].toDouble() },
                    c[u"width"_s].toDouble(), c[u"height"_s].toDouble(),
                    c[u"progress"_s].toDouble());
            maximum = std::max(
                    { maximum,
                      errorMagnitude((actual.position - vector(e[u"position"_s])).length()),
                      errorMagnitude((actual.aim - vector(e[u"aim"_s])).length()),
                      errorMagnitude(actual.span - e[u"span"_s].toDouble()) });
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
    void collidingEqualPriorityLabelsPreserveInputOrder()
    {
        QVector<OverviewColumn> candidates{
            { 99, u"invalid"_s, 3, std::numeric_limits<double>::quiet_NaN(), 300, 1, true },
            { 7, u"first"_s, 3, 500, 300, 1, false },
            { 8, u"second"_s, 3, 500, 300, 1, false }
        };
        auto visible = OverviewLabelsModel::visibleLabels(candidates, 1600, 900);
        QCOMPARE(visible.size(), 1);
        QCOMPARE(visible.first().lane, 7);
        std::swap(candidates[1], candidates[2]);
        visible = OverviewLabelsModel::visibleLabels(candidates, 1600, 900);
        QCOMPARE(visible.size(), 1);
        QCOMPARE(visible.first().lane, 8);
    }
    void frozenProductionProjection()
    {
        Fixture f;
        QVERIFY(f.selection.selectCell({ 2, 48 }, true));
        f.motion.finish();
        double maximum = 0;
        for (const auto value : golden()[u"projections"_s].toArray()) {
            const auto s = value.toObject();
            f.selection.setRealistic(s[u"mode"_s] == u"realistic"_s);
            f.motion.finish();
            f.selection.advance(10);
            f.scene.layout()->setViewport(s[u"width"_s].toDouble(), s[u"height"_s].toDouble());
            const auto pose = OverviewController::framing({ { 0, 0, 140 }, { 0, 0, 0 }, 7.33 },
                                                          s[u"width"_s].toDouble(),
                                                          s[u"height"_s].toDouble(), 1);
            // Projection fixture pins the wide camera independently of motion progress.
            f.scene.observe(pose, 0);
            f.overview.setActive(true);
            // Reach each exact source progress through the actual controller's rate-6.5 trajectory.
            f.overview.reset();
            f.overview.setActive(true);
            const double target = s[u"progress"_s].toDouble();
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
            const auto expected = columns(s[u"columns"_s].toArray());
            QCOMPARE(actual.size(), expected.size());
            for (int i = 0; i < actual.size(); ++i) {
                QCOMPARE(actual[i].lane, expected[i].lane);
                QCOMPARE(actual[i].count, expected[i].count);
                QVERIFY(errorMagnitude(actual[i].extent - expected[i].extent) <= 1e-6);
                maximum = std::max({ maximum, errorMagnitude(actual[i].x - expected[i].x),
                                     errorMagnitude(actual[i].y - expected[i].y) });
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
        OverviewColumn column{ 1, u"\u7b2c\u4e00\u5217"_s, 8, 800, 400, 1, true };
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
                QVERIFY(point);
                QVERIFY(point->x() >= 0 && point->x() < 1280 && point->y() >= 0
                        && point->y() < 720);
                auto isolated = f.scene.frame();
                isolated.instances = { instance };
                QVERIFY(f.selection.hitTest(isolated, point->x(), point->y()).isEmpty());
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
