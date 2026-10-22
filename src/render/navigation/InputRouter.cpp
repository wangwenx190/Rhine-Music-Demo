#include "SceneGeometry.h"
#include "Units.h"
#include <QQuickWindow>
#include <QWheelEvent>
#include "SelectionController.h"
#include "InputRouter.h"
#include <QKeyEvent>
#include <QQuickItem>
#include <algorithm>
#include <cmath>

namespace yalahoo {
namespace {
// Normalize wheel units once and bound gesture accumulation before issuing navigation intent.
constexpr auto WheelBurstGapMilliseconds = 200;
constexpr auto MaximumWheelDeltaPixels = 144.0;
constexpr auto PixelsPerWheelStep = 48;
constexpr auto MaximumLaneMagnitude = 1e9;
constexpr auto ArchiveRowEndOffset = 11.0;
constexpr auto MaximumRowMagnitude = 1e12;
constexpr auto WheelStepPixels = 48.0;
constexpr auto AngleDeltaPerDetent = 120;
constexpr auto MinimumSwipePixels = 36;
constexpr auto SwipeAxisDominance = 1.3;
constexpr auto MaximumSwipeMilliseconds = 1400;

bool inputOwner(QQuickItem *item, bool wheel)
{
    return item->property("navigationInputOwner").toBool() || item->inherits("QQuickTextInput")
            || item->inherits("QQuickTextEdit") || item->inherits("QQuickSlider")
            || item->inherits("QQuickSpinBox") || item->inherits("QQuickComboBox")
            || item->inherits("QQuickPopupItem")
            || (wheel
                && (item->inherits("QQuickFlickable") || item->inherits("QQuickAbstractButton")));
}
bool ownsInput(QQuickItem *item, bool wheel)
{
    for (auto *node = item; node; node = node->parentItem()) {
        if (inputOwner(node, wheel))
            return true;
    }
    return false;
}
bool pointerOwner(QQuickItem *item, QPointF scenePoint, bool inheritedOwner = false)
{
    if (!item->isVisible() || !item->isEnabled() || item->opacity() <= 0)
        return false;
    const bool contains = item->contains(item->mapFromScene(scenePoint));
    if (item->clip() && !contains)
        return false;
    // Carry ancestor ownership down this event's traversal instead of walking the same
    // parent chain again for every overlapping item. No state survives the input event.
    const bool owned = inheritedOwner || inputOwner(item, true);
    if (contains && owned)
        return true;
    for (auto *child : item->childItems())
        if (pointerOwner(child, scenePoint, owned))
            return true;
    return false;
}
}
InputRouter::InputRouter(SelectionController *selection, QObject *parent)
    : QObject(parent), m_selection(selection)
{
    m_timer.start();
    if (selection) {
        connect(selection, &SelectionController::selectionChanged, this, [this] {
            if (!m_committing) {
                m_pending.reset();
                setNavigationDirection(0, false);
            }
            const auto &pool = m_selection->pool();
            const int context = ArchivePool::wrap(m_selection->cell().lane, pool.columnCount());
            if (context != m_context)
                reset();
            m_context = context;
        });
        const auto invalidate = [this] {
            m_pending.reset();
            setNavigationDirection(0, false);
            reset();
        };
        connect(selection, &SelectionController::libraryChanged, this, invalidate);
        connect(selection, &SelectionController::arrayModeChanged, this, invalidate);
        connect(selection, &SelectionController::coordinatesRebased, this,
                [this](double lane, double row) {
                    if (m_pending) {
                        m_pending->lane -= lane;
                        m_pending->row -= row;
                    }
                });
    }
}
void InputRouter::attach(QQuickWindow *window)
{
    if (m_window)
        m_window->removeEventFilter(this);
    m_window = window;
    if (window)
        window->installEventFilter(this);
}
bool InputRouter::enabled() const
{
    return m_enabled;
}
void InputRouter::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    m_pending.reset();
    reset();
    Q_EMIT enabledChanged();
}
void InputRouter::reset()
{
    m_remainder = 0;
    m_direction = 0;
    m_lastTime = -yalahoo::inputTiming::InactiveGestureTime;
}
int InputRouter::consume(double delta, double milliseconds)
{
    if (!std::isfinite(delta) || !std::isfinite(milliseconds) || delta == 0)
        return 0;
    const int direction = delta > 0 ? 1 : -1;
    if (milliseconds - m_lastTime > WheelBurstGapMilliseconds || milliseconds < m_lastTime
        || direction != m_direction)
        m_remainder = 0;
    m_lastTime = milliseconds;
    m_direction = direction;
    m_remainder += std::min(std::abs(delta), MaximumWheelDeltaPixels) * direction;
    const int steps = int(m_remainder / PixelsPerWheelStep);
    m_remainder -= steps * PixelsPerWheelStep;
    return steps;
}
bool InputRouter::commit(int direction, bool column)
{
    if (!m_enabled || !m_selection || direction == 0)
        return false;
    Q_EMIT inputReceived();
    setNavigationDirection(direction, column);
    // Finite columns clamp the requested endpoint, including oversized wheel/ruler steps.
    const auto base = m_pending.value_or(m_selection->cell());
    if (column) {
        if ((direction != -1 && direction != 1) || m_selection->pool().columnCount() < 2
            || std::abs(base.lane + direction) > MaximumLaneMagnitude)
            return false;
        const auto target = m_selection->columnTarget(int(base.lane + direction));
        return target && queue(*target);
    }
    return queue({ base.lane, base.row + direction });
}
bool InputRouter::selectRow(double row)
{
    m_pending.reset();
    if (m_selection)
        setNavigationDirection((row > m_selection->cell().row) - (row < m_selection->cell().row),
                               false);
    return m_selection && queue({ m_selection->cell().lane, row });
}
bool InputRouter::queue(ArchiveCell target)
{
    if (!m_enabled || !m_selection || !std::isfinite(target.row))
        return false;
    const int count = m_selection->pool().rowCount(target.lane);
    if (!count)
        return false;
    if (m_selection->realistic())
        target.row = std::clamp(target.row, double(yalahoo::sceneGeometry::FirstAlbumRow),
                                ArchiveRowEndOffset + count);
    if (std::floor(target.row) != target.row || std::abs(target.row) > MaximumRowMagnitude)
        return false;
    m_pending = target;
    flush();
    return true;
}
void InputRouter::flush()
{
    if (!m_pending || !m_enabled || !m_selection)
        return;
    const auto target = *m_pending;
    // At most one replaceable destination waits for bounded returning-cover ownership.
    m_committing = true;
    const bool moved = m_selection->selectCell(target);
    m_committing = false;
    if (moved) {
        m_pending.reset();
        Q_EMIT navigationCommitted(m_selection->revision());
    }
}
bool InputRouter::row(int direction)
{
    reset();
    return commit(direction, false);
}
bool InputRouter::lane(int direction)
{
    reset();
    return commit(direction, true);
}
bool InputRouter::wheel(QWheelEvent *event, QQuickWindow *window)
{
    if (!m_enabled || !m_selection || !window
        || pointerOwner(window->contentItem(), event->position())
        || event->modifiers().testFlag(Qt::ControlModifier)
        || event->modifiers().testFlag(Qt::ShiftModifier)) {
        reset();
        return false;
    }
    const QPointF delta = !event->pixelDelta().isNull()
            ? QPointF(event->pixelDelta())
            : QPointF(event->angleDelta()) * (WheelStepPixels / AngleDeltaPerDetent);
    if (std::abs(delta.x()) >= std::abs(delta.y()) || delta.y() == 0) {
        reset();
        return false;
    }
    if (event->phase() == Qt::ScrollBegin)
        reset();
    const int steps =
            consume(-delta.y(), m_timer.nsecsElapsed() / yalahoo::units::NanosecondsPerMillisecond);
    if (steps)
        commit(steps, false);
    if (event->phase() == Qt::ScrollEnd)
        reset();
    return true;
}
bool InputRouter::ownsFocus() const
{
    return m_window && ownsInput(m_window->activeFocusItem(), false);
}
bool InputRouter::key(QKeyEvent *event, QQuickWindow *window)
{
    reset();
    if (!m_enabled || !window || ownsInput(window->activeFocusItem(), false)
        || event->modifiers().testFlag(Qt::ControlModifier)
        || event->modifiers().testFlag(Qt::MetaModifier)
        || event->modifiers().testFlag(Qt::AltModifier))
        return false;
    switch (event->key()) {
    case Qt::Key_V:
        Q_EMIT overviewToggleRequested();
        return true;
    case Qt::Key_Escape:
        Q_EMIT overviewReturnRequested();
        return false;
    case Qt::Key_Up:
        commit(-1, false);
        return true;
    case Qt::Key_Down:
        commit(1, false);
        return true;
    case Qt::Key_Left:
        commit(-1, true);
        return true;
    case Qt::Key_Right:
        commit(1, true);
        return true;
    default:
        return false;
    }
}
bool InputRouter::swipe(QPointF delta, double milliseconds)
{
    reset();
    const double major = std::max(std::abs(delta.x()), std::abs(delta.y()));
    const double minor = std::min(std::abs(delta.x()), std::abs(delta.y()));
    if (!std::isfinite(major) || !std::isfinite(milliseconds) || milliseconds < 0
        || major < MinimumSwipePixels || major < minor * SwipeAxisDominance
        || milliseconds > MaximumSwipeMilliseconds)
        return false;
    const bool column = std::abs(delta.x()) > std::abs(delta.y());
    return commit((column ? delta.x() : delta.y()) < 0 ? 1 : -1, column);
}
double InputRouter::remainder() const
{
    return m_remainder;
}
int InputRouter::rowDirection() const
{
    return m_rowDirection;
}
int InputRouter::columnDirection() const
{
    return m_columnDirection;
}
int InputRouter::navigationDirection() const
{
    return m_navigationDirection;
}
void InputRouter::setNavigationDirection(int direction, bool column)
{
    direction = (direction > 0) - (direction < 0);
    const int row = column ? 0 : direction;
    const int lane = column ? direction
            : m_pending && m_selection && m_pending->lane != m_selection->cell().lane
            ? m_columnDirection
            : 0;
    if (m_rowDirection == row && m_columnDirection == lane && m_navigationDirection == direction)
        return;
    m_rowDirection = row;
    m_columnDirection = lane;
    m_navigationDirection = direction;
    Q_EMIT directionChanged();
}
bool InputRouter::eventFilter(QObject *object, QEvent *event)
{
    if (object != m_window)
        return false;
    if (event->type() == QEvent::KeyPress)
        return key(static_cast<QKeyEvent *>(event), m_window);
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::TouchBegin
        || event->type() == QEvent::WindowDeactivate || event->type() == QEvent::Hide)
        reset();
    return false;
}
}
