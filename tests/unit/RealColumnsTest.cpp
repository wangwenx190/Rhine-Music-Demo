#include "SelectionController.h"
#include "PreferencesStore.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QTest>
#include <QSignalSpy>
#include <QSet>
#include <cmath>
using namespace yalahoo;
namespace {
QList<LibraryColumn> columns(const QList<int> &counts)
{
    QList<LibraryColumn> result;
    for (int lane = 0; lane < counts.size(); ++lane) {
        LibraryColumn column;
        column.key = QString::number(lane);
        for (int row = 0; row < counts[lane]; ++row)
            column.albumIds.append(QStringLiteral("%1:%2").arg(lane).arg(row));
        result.append(column);
    }
    return result;
}
}
class RealColumnsTest : public QObject
{
    Q_OBJECT
private slots:
    void duplicateIdentitiesAndInvalidReset()
    {
        auto input = columns({ 2, 3 });
        input[1].albumIds.append(input[0].albumIds.first());
        input[0].albumIds.append(input[0].albumIds.first());
        input[0].albumIds.append(QString());
        MotionController motion;
        SelectionController selection(&motion);
        selection.setRealistic(true);
        selection.setColumns(input);
        QCOMPARE(selection.pool().rowCount(0), 2);
        QCOMPARE(selection.pool().rowCount(1), 3);
        QSet<QString> identities;
        for (const auto &instance : selection.instances()) {
            QVERIFY(!identities.contains(instance.albumId));
            identities.insert(instance.albumId);
        }
        QCOMPARE(identities.size(), 5);
        RealColumns real;
        real.reset({ 0, 3 }, { 0, 99 });
        QCOMPARE(real.row(1), 14);
        QCOMPARE(real.activity(1), 1);
        real.reset({ }, { 0, 99 });
        QCOMPARE(real.row(0), 12);
    }
    void unaffectedWaveAndLazyColumns()
    {
        MotionController firstMotion, secondMotion;
        SelectionController first(&firstMotion), second(&secondMotion);
        for (auto *selection : { &first, &second }) {
            selection->setRealistic(true);
            selection->setColumns(columns({ 3, 49, 106, 7 }));
            selection->setLift(0);
        }
        QVERIFY(first.select("1:40"));
        first.advance(.3);
        second.advance(.3);
        QCOMPARE(first.field(15, 3), second.field(15, 3));
        QCOMPARE(first.browseRow(3), 15);
        QVERIFY(first.select("2:105"));
        first.advance(.3);
        second.advance(.3);
        QCOMPARE(first.field(16, 3), second.field(16, 3));
        RealColumns real;
        real.reset({ 3, 106 }, { 0, 12 });
        real.select({ 1, 117 }, true);
        real.update(.2, false);
        real.update(0, true);
        real.update(.5, false);
        QCOMPARE(real.row(1), 117);
        QCOMPARE(real.row(0), 13);
        QVERIFY(real.settled(1));
        QCOMPARE(real.row(1, 1), 64.5);
        real.select({ 1, 12 }, false, true);
        real.update(4, false);
        QVERIFY(std::abs(real.row(1) - 12) <= 1e-6);
    }
    void frozenTrajectories()
    {
        const QString full = qEnvironmentVariable("YALAHOO_REAL_COLUMN_GOLDEN");
        QFile input(full.isEmpty()
                            ? QStringLiteral(FIXTURE_ROOT
                                             "/tests/fixtures/real-columns/trajectories.json")
                            : full);
        QVERIFY(input.open(QIODevice::ReadOnly));
        const auto golden = QJsonDocument::fromJson(input.readAll()).object();
        QList<int> counts;
        for (auto count : golden["counts"].toArray())
            counts.append(count.toInt());
        double error = 0;
        for (auto value : golden["cases"].toArray()) {
            const auto run = value.toObject();
            RealColumns real;
            real.reset(counts, { 1, 12 });
            const double dt = run["speed"].toDouble() / run["fps"].toDouble();
            for (auto segmentValue : run["segments"].toArray()) {
                const auto segment = segmentValue.toObject();
                const auto action = segment["action"].toObject();
                real.select({ action["lane"].toDouble(), action["row"].toDouble() },
                            action["guided"].toBool(), action["detail"].toBool());
                int step = 0;
                for (auto sampleValue : segment["samples"].toArray()) {
                    const auto sample = sampleValue.toObject();
                    while (step < sample["step"].toInt()) {
                        real.update(dt, action["reduced"].toBool());
                        ++step;
                    }
                    const auto states = sample["state"].toArray();
                    for (int lane = 0; lane < counts.size(); ++lane) {
                        const auto row = real.rowState(lane), activity = real.activityState(lane);
                        const std::array<double, 6> actual{
                            row.value,      row.velocity,      row.acceleration,
                            activity.value, activity.velocity, activity.acceleration
                        };
                        const auto expected = states[lane].toArray();
                        for (int i = 0; i < 6; ++i)
                            error = std::max(error, std::abs(actual[i] - expected[i].toDouble()));
                    }
                }
            }
        }
        qInfo("Maximum frozen x/v/a error: %.12g", error);
        QVERIFY(error <= 1e-6);
    }
    void finiteBoundariesAndIdentity()
    {
        MotionController motion;
        motion.setReducedMotion(true);
        SelectionController selection(&motion);
        selection.setRealistic(true);
        selection.setColumns(columns({ 0, 1, 2, 3, 49, 106 }));
        QCOMPARE(selection.albumId(), QStringLiteral("1:0"));
        QVERIFY(!selection.canMoveNext());
        QVERIFY(!selection.canMovePrevious());
        const auto revision = selection.revision();
        QVERIFY(!selection.moveRow(1));
        QCOMPARE(selection.revision(), revision);
        QVERIFY(selection.select("2:1"));
        QVERIFY(!selection.moveRow(1));
        QVERIFY(selection.moveRow(-1));
        QVERIFY(!selection.moveRow(-1));
        QVERIFY(!selection.navigateRow(11));
        QVERIFY(!selection.selectCell({ 2, 14 }));
        QVERIFY(selection.select("5:105"));
        QVERIFY(!selection.canMoveNext());
        QVERIFY(selection.moveLane(-1));
        // Empty columns are retained in direct test inputs, never appear in production grouping.
        QVERIFY(selection.select("4:24"));
        selection.advance(0);
        QSet<QString> ids;
        for (const auto &instance : selection.instances()) {
            QVERIFY(!ids.contains(instance.albumId));
            ids.insert(instance.albumId);
            QVERIFY(selection.pool().realAlbumAt(instance.cell) == instance.albumId);
        }
        QVERIFY(selection.instances().size() <= 432 + 4);
        selection.setColumns({ });
        QVERIFY(selection.instances().isEmpty());
        QVERIFY(!selection.moveLane(1));
    }
    void memoryAndActualDisplayedDepth()
    {
        MotionController motion;
        SelectionController selection(&motion);
        selection.setRealistic(true);
        selection.setColumns(columns({ 3, 106, 49 }));
        selection.setLift(0);
        QVERIFY(selection.select("1:80"));
        selection.advance(2.4);
        QVERIFY(selection.moveLane(1));
        selection.advance(.2);
        const auto unfinished = selection.realColumns().rowState(1);
        QVERIFY(unfinished.value > 64.5);
        const auto image = selection.instances();
        const auto id = selection.albumId();
        selection.setRememberColumnPosition(false);
        QCOMPARE(selection.instances(), image);
        QCOMPARE(selection.albumId(), id);
        const int displayed = int(std::floor(unfinished.value + .5));
        QVERIFY(selection.moveLane(-1));
        QCOMPARE(selection.cell().row, displayed);
        QCOMPARE(selection.realColumns().rowState(1).value, unfinished.value);
        QCOMPARE(selection.realColumns().rowState(1).velocity, unfinished.velocity);
        QCOMPARE(selection.realColumns().rowState(1).acceleration, unfinished.acceleration);
        selection.setRememberColumnPosition(true);
        QVERIFY(selection.moveLane(1));
        QVERIFY(selection.moveLane(-1));
        QCOMPARE(selection.cell().row, displayed);
        selection.setRememberColumnPosition(false);
        selection.advance(3);
        QVERIFY(selection.enterColumn(2));
        QCOMPARE(selection.cell().row, 36);
        QVERIFY(selection.enterColumn(0));
        QCOMPARE(selection.cell().row, 13);
        const auto before = selection.albumId();
        selection.setRealistic(false);
        QCOMPARE(selection.albumId(), before);
        selection.setLift(0);
        QVERIFY(selection.navigateRow(-100));
        motion.finish();
        selection.advance(10);
        QVERIFY(selection.moveLane(1));
        QCOMPARE(selection.cell().row, -100);
    }
    void preferencePersistenceAndCorruption()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString path = root.filePath("preferences.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\"futureField\":{\"keep\":true}}");
        file.close();
        {
            PreferencesStore preferences(root.path());
            QVERIFY(preferences.rememberColumnPosition());
            preferences.setNavigation(false, true);
            preferences.setNavigation(true, false);
            preferences.setNavigation(false, true);
        }
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto saved = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        QVERIFY(!saved["rememberColumnPosition"].toBool());
        QCOMPARE(saved["arrayMode"].toString(), QStringLiteral("realistic"));
        QVERIFY(saved["futureField"].toObject()["keep"].toBool());
        QVERIFY(!saved.contains("columnMemory"));
        {
            PreferencesStore reloaded(root.path());
            QVERIFY(!reloaded.rememberColumnPosition());
            QVERIFY(reloaded.realistic());
        }
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write("broken");
        file.close();
        {
            PreferencesStore corrupt(root.path());
            QVERIFY(!corrupt.error().isEmpty());
            QSignalSpy errors(&corrupt, &PreferencesStore::saveError);
            corrupt.setNavigation(false, true);
            QCOMPARE(errors.size(), 1);
        }
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("broken"));
    }
};
QTEST_GUILESS_MAIN(RealColumnsTest)
#include "RealColumnsTest.moc"
