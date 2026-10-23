// Checks ruler geometry, wheel normalization and decimal trajectory state using deterministic
// inputs. Explicit fixture literals are independent inputs or expected results; timing bounds
// have named constants.

#include "FrameClock.h"
#include "MotionController.h"
#include "SelectionController.h"
#include <QWheelEvent>
#include <QQuickWindow>
#include "InputRouter.h"
#include "RulerController.h"
#include "OrdinalMotion.h"
#include "VerificationWindow.h"
#include <QtTest>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QScopeGuard>
#include <limits>
#include <vector>

using namespace Qt::StringLiterals;

using namespace yalahoo;
namespace {
void populate(SelectionController &selection)
{
    QList<LibraryColumn> columns;
    for (int lane = 0; lane < 2; ++lane) {
        LibraryColumn column;
        column.key = QString::number(lane);
        for (int row = 0; row < (lane == 0 ? 37 : 3); ++row)
            column.albumIds.append(u"a%1-%2"_s.arg(lane).arg(row));
        columns.append(column);
    }
    selection.setColumns(columns);
    selection.setLift(0);
}
}
class NavigationTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(NavigationTest)
public:
    NavigationTest() = default;

private Q_SLOTS:
    void rulerKeepsFullAccessibleTitleSeparateFromCoverBudget()
    {
        MotionController motion;
        SelectionController selection(&motion);
        LibrarySnapshot library;
        Album album;
        album.id = u"one"_s;
        album.title = QString(300, u'x') + u"\U0001f3b5"_s;
        library.albums.append(album);
        selection.setColumns({ LibraryColumn{ u"genre"_s, u"Genre"_s, { album.id } } },
                             std::make_shared<const LibrarySnapshot>(library));
        RulerController ruler(&motion, &selection);
        QCOMPARE(selection.albumTitle(album.id), album.title);
        QVERIFY(selection.coverSource(album.id).placeholderTitle.size() <= 256);
        bool found = false;
        for (const auto &value : ruler.slotStates()) {
            const auto state = value.toMap();
            if (state.value(u"active"_s).toBool()) {
                QCOMPARE(state.value(u"title"_s).toString(), album.title);
                found = true;
            }
        }
        QVERIFY(found);
        ruler.configure(37, 5, 5, true, true);
        QVERIFY(!ruler.slotStates().isEmpty());
    }
    void rulerReusesUnchangedSlotsAndRefreshesTitles()
    {
        MotionController motion;
        motion.setReducedMotion(true);
        SelectionController selection(&motion);
        LibrarySnapshot library;
        LibraryColumn column{ u"genre"_s, u"Genre"_s, { } };
        for (int i = 0; i < 3; ++i) {
            Album album;
            album.id = QString::number(i);
            album.title = u"Title %1"_s.arg(i);
            library.albums.append(album);
            column.albumIds.append(album.id);
        }
        selection.setColumns({ column }, std::make_shared<const LibrarySnapshot>(library));
        RulerController ruler(&motion, &selection);
        const auto before = ruler.slotStates();
        int hovered = -1;
        for (int i = 0; i < before.size(); ++i)
            if (before[i].toMap().value(u"enabled"_s).toBool()
                && !before[i].toMap().value(u"active"_s).toBool()) {
                hovered = i;
                break;
            }
        QVERIFY(hovered >= 0);
        ruler.hover(hovered, true, false);
        const auto after = ruler.slotStates();
        for (int i = 0; i < before.size(); ++i) {
            const auto old = before[i].toMap();
            const auto next = after[i].toMap();
            if (i != hovered) {
                QCOMPARE(next, old);
                QVERIFY(next.isSharedWith(old));
            }
        }
        QVERIFY(after[hovered].toMap().value(u"hovering"_s).toBool());
        QVERIFY(!before[hovered].toMap().value(u"hovering"_s).toBool());
        library.albums[0].title = u"Updated title"_s;
        selection.setColumns({ column }, std::make_shared<const LibrarySnapshot>(library));
        bool found = false;
        for (const auto &value : ruler.slotStates()) {
            const auto slot = value.toMap();
            if (slot.value(u"active"_s).toBool()) {
                QCOMPARE(slot.value(u"title"_s).toString(), u"Updated title"_s);
                found = true;
            }
        }
        QVERIFY(found);
    }
    void wheelRemainder()
    {
        InputRouter input(nullptr);
        QCOMPARE(input.consume(20, 0), 0);
        QCOMPARE(input.consume(27, 50), 0);
        QCOMPARE(input.consume(1, 100), 1);
        QCOMPARE(input.remainder(), 0);
        QCOMPARE(input.consume(47, 101), 0);
        QCOMPARE(input.consume(-1, 102), 0);
        QCOMPARE(input.remainder(), -1);
        QCOMPARE(input.consume(-47, 303), 0);
        QCOMPARE(input.consume(-1, 304), -1);
        QCOMPARE(input.consume(-1, 1), 0);
        QCOMPARE(input.consume(100000, 2), 3);
        QVERIFY(std::abs(input.remainder()) < 48);
        QCOMPARE(input.consume(std::numeric_limits<double>::infinity(), 3), 0);
    }
    void boundariesAndTouch()
    {
        MotionController motion;
        SelectionController selection(&motion);
        populate(selection);
        selection.setRealistic(true);
        InputRouter input(&selection);
        QVERIFY(input.row(-3));
        QCOMPARE(selection.cell().row, 12);
        QVERIFY(input.row(100));
        QCOMPARE(selection.cell().row, 48);
        QVERIFY(input.row(-100));
        QCOMPARE(selection.cell().row, 12);
        QVERIFY(!input.swipe({ 35, 0 }, 100));
        QVERIFY(!input.swipe({ 36, 30 }, 100));
        QVERIFY(!input.swipe({ 0, -36 }, 1401));
        QVERIFY(input.swipe({ 0, -36 }, 1400));
        QCOMPARE(selection.cell().row, 13);
        QVERIFY(input.swipe({ -36, 0 }, 100));
        QCOMPARE(selection.cell().lane, 1);
        QVERIFY(input.row(3));
        QCOMPARE(selection.cell().row, 14);
        selection.setRealistic(false);
        QVERIFY(input.row(3));
        QCOMPARE(selection.cell().row, 17);
    }
    void latestTargetAndReverse()
    {
        MotionController motion;
        SelectionController selection(&motion);
        populate(selection);
        selection.setLift(.9);
        InputRouter input(&selection);
        for (int i = 0; i < 30; ++i) {
            if (i < 4)
                selection.setLift(.9);
            QVERIFY(input.row(1));
        }
        QCOMPARE(selection.albumId(), u"a0-3"_s);
        for (int i = 0; i < 13; ++i)
            QVERIFY(input.row(-1));
        for (int frame = 0; frame < 480; ++frame) {
            selection.advance(1.0 / 120);
            input.flush();
        }
        QCOMPARE(selection.albumId(), u"a0-17"_s);
        QVERIFY(selection.instances().size() <= 436);
    }
    void pendingColumnAndRow()
    {
        MotionController motion;
        SelectionController selection(&motion);
        populate(selection);
        InputRouter input(&selection);
        for (int i = 0; i < 4; ++i) {
            selection.setLift(.9);
            QVERIFY(input.row(1));
        }
        QCOMPARE(selection.albumId(), u"a0-3"_s);
        QVERIFY(input.lane(1));
        QVERIFY(input.row(1));
        QVERIFY(input.lane(-1));
        QVERIFY(input.lane(1));
        QVERIFY(input.row(2));
        for (int frame = 0; frame < 480; ++frame) {
            selection.advance(1.0 / 120);
            input.flush();
        }
        QCOMPARE(selection.albumId(), u"a1-2"_s);
        QVERIFY(selection.instances().size() <= 436);
    }
    void rulerOccurrenceAndRebase()
    {
        MotionController motion;
        motion.setReducedMotion(true);
        SelectionController selection(&motion);
        populate(selection);
        InputRouter input(&selection);
        RulerController ruler(&motion, &selection);
        ruler.setInputRouter(&input);
        const auto states = ruler.slotStates();
        int previous = -1;
        for (int i = 0; i < 16; ++i)
            if (states[i].toMap()[u"ordinal"_s].toDouble() == -1)
                previous = i;
        QVERIFY(previous >= 0);
        QVERIFY(ruler.click(previous));
        QCOMPARE(selection.albumId(), u"a0-36"_s);
        QCOMPARE(ruler.state()[u"selected"_s].toDouble(), -1);
        const double before = ruler.state()[u"selected"_s].toDouble();
        selection.selectCell({ 0, 1000000 });
        const auto target = ruler.state()[u"selected"_s].toDouble();
        selection.rebase();
        QCOMPARE(ruler.state()[u"selected"_s].toDouble(), target);
        QVERIFY(target != before);
        QVERIFY(input.row(1));
        QCOMPARE(ruler.state()[u"selected"_s].toDouble(), target + 1);
        selection.setRealistic(true);
        QCOMPARE(ruler.state()[u"selected"_s].toDouble(), selection.cell().row - 12);
        ruler.setActive(false);
        for (int i = 0; i < 16; ++i)
            QVERIFY(!ruler.slotStates()[i].toMap()[u"enabled"_s].toBool());
    }
    void decimalDirectionOwnership()
    {
        MotionController motion;
        SelectionController selection(&motion);
        populate(selection);
        selection.setRealistic(true);
        InputRouter input(&selection);
        QVERIFY(input.lane(-1));
        QCOMPARE(selection.cell().lane, 1);
        QCOMPARE(input.columnDirection(), -1);
        QCOMPARE(input.rowDirection(), 0);
        QCOMPARE(input.navigationDirection(), -1);
        QVERIFY(input.row(1));
        QCOMPARE(input.rowDirection(), 1);
        QCOMPARE(input.columnDirection(), 0);
        QVERIFY(selection.navigateRow(12));
        QCOMPARE(input.navigationDirection(), 0);
        selection.setRealistic(false);
        for (int i = 0; i < 4; ++i) {
            selection.setLift(.9);
            QVERIFY(input.row(1));
        }
        QVERIFY(input.lane(-1));
        QVERIFY(input.row(1));
        QCOMPARE(input.columnDirection(), -1);
        QCOMPARE(input.rowDirection(), 1);
        QCOMPARE(input.navigationDirection(), 1);
    }
    void ownersAndPixelPriority()
    {
        MotionController motion;
        SelectionController selection(&motion);
        populate(selection);
        InputRouter input(&selection);
        QQuickWindow window;
        verification::prepareWindow(window);
        window.resize(300, 200);
        input.attach(&window);
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData("import QtQuick\nTextInput { width: 100; height: 50; text: 'x' }"_ba,
                          QUrl());
        std::unique_ptr<QObject> object(component.create());
        auto *text = qobject_cast<QQuickItem *>(object.get());
        QVERIFY(text);
        text->setParentItem(window.contentItem());
        text->forceActiveFocus();
        QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
        const auto before = selection.albumId();
        QVERIFY(!input.key(&down, &window));
        QCOMPARE(selection.albumId(), before);
        QWheelEvent owned({ 10, 10 }, { 10, 10 }, { 0, -48 }, { 0, -120 }, Qt::NoButton,
                          Qt::NoModifier, Qt::NoScrollPhase, false);
        QVERIFY(!input.wheel(&owned, &window));
        QCOMPARE(selection.albumId(), before);
        text->setVisible(false);
        QWheelEvent small({ 150, 100 }, { 150, 100 }, { 0, -12 }, { 0, -120 }, Qt::NoButton,
                          Qt::NoModifier, Qt::NoScrollPhase, false);
        QVERIFY(input.wheel(&small, &window));
        QCOMPARE(selection.albumId(), before);
        for (int i = 0; i < 3; ++i)
            QVERIFY(input.wheel(&small, &window));
        QCOMPARE(selection.cell().row, 13);
        QWheelEvent horizontal({ 150, 100 }, { 150, 100 }, { 48, 0 }, { 120, 0 }, Qt::NoButton,
                               Qt::NoModifier, Qt::NoScrollPhase, false);
        QVERIFY(!input.wheel(&horizontal, &window));
        QCOMPARE(input.remainder(), 0);
        QVERIFY(!window.isVisible());
        QVERIFY(verification::windowPolicySatisfied(window));
    }
    void deepPointerOwnersKeepClipAndSiblingIsolation()
    {
        MotionController motion;
        SelectionController selection(&motion);
        populate(selection);
        InputRouter input(&selection);
        QQuickWindow window;
        verification::prepareWindow(window);
        window.resize(300, 200);
        std::vector<std::unique_ptr<QQuickItem>> nodes;
        const auto cleanup = qScopeGuard([&] {
            while (!nodes.empty())
                nodes.pop_back();
        });
        auto *parent = window.contentItem();
        // Zero-sized, unclipped ancestors may contain a descendant under the pointer.
        for (int i = 0; i < 512; ++i) {
            auto item = std::make_unique<QQuickItem>(parent);
            parent = item.get();
            nodes.push_back(std::move(item));
        }
        auto *branch = nodes.front().get();
        branch->setProperty("navigationInputOwner", true);
        parent->setSize({ 100, 50 });
        QQuickItem sibling(window.contentItem());
        sibling.setSize({ 300, 200 });
        const auto blocked = [&] {
            QWheelEvent wheel({ 10, 10 }, { 10, 10 }, { 0, -1 }, { }, Qt::NoButton, Qt::NoModifier,
                              Qt::NoScrollPhase, false);
            return !input.wheel(&wheel, &window);
        };
        QVERIFY(blocked());
        branch->setClip(true);
        QVERIFY(!blocked());
        branch->setClip(false);
        QVERIFY(blocked());
        branch->setVisible(false);
        QVERIFY(!blocked());
        branch->setVisible(true);
        branch->setEnabled(false);
        QVERIFY(!blocked());
        branch->setEnabled(true);
        branch->setOpacity(0);
        QVERIFY(!blocked());
        branch->setOpacity(1);
        branch->setProperty("navigationInputOwner", false);
        QVERIFY(!blocked());
        parent->setProperty("navigationInputOwner", true);
        QVERIFY(blocked());
        // The cleanup guard also avoids recursive tree destruction on failed assertions.
        QVERIFY(!window.isVisible());
    }
    void ordinalContinuityAndClock()
    {
        OrdinalTrack track;
        track.retarget(1, true);
        track.retarget(7, false);
        track.advance(.123);
        const double position = track.value;
        const double velocity = track.velocity;
        track.retarget(-2, false);
        QCOMPARE(track.points[0], position);
        QVERIFY(velocity > 0);
        track.advance(.460);
        QCOMPARE(track.value, -2);
        QCOMPARE(track.velocity, 0);
        FrameClock clock;
        OrdinalMotion motion;
        motion.setClock(&clock);
        motion.setValue(9);
        motion.setValue(10);
        QCOMPARE(motion.faces().size(), 2);
        const auto partial = motion.faces();
        motion.setActive(false);
        motion.advance(.460);
        QCOMPARE(motion.faces(), partial);
        motion.setActive(true);
        motion.setReducedMotion(true);
        QCOMPARE(motion.faces()[0].toMap()[u"upper"_s].toInt(), 1);
        QCOMPARE(motion.faces()[1].toMap()[u"upper"_s].toInt(), 0);
        QCOMPARE(motion.faces()[1].toMap()[u"fraction"_s].toDouble(), 0);
    }
};
QTEST_MAIN(NavigationTest)
#include "NavigationTest.moc"
