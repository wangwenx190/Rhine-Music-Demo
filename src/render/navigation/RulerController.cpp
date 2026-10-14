#include "RulerController.h"
#include "InputRouter.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace yalahoo {
namespace {
bool approach(ScalarSpring &s, double target, double dt, bool instant)
{
    if (instant || (std::abs(s.value - target) < .001 && std::abs(s.velocity) < .01)) {
        s.finish(target);
        return false;
    }
    s.update(target, 18, dt);
    return true;
}
}
RulerController::RulerController(MotionController *motion, SelectionController *selection,
                                 QObject *parent)
    : QObject(parent), m_motion(motion), m_selection(selection)
{
    for (int i = 0; i < 16; ++i) {
        m_slots[i].ordinal = i;
        m_slots[i].x = i * 11;
        m_slots[i].height.finish(10);
        m_slots[i].opacity.finish(.28);
    }
    if (selection) {
        connect(selection, &SelectionController::selectionChanged, this, [this] {
            synchronize();
        });
        connect(selection, &SelectionController::libraryChanged, this, [this] {
            synchronize(true);
        });
        connect(selection, &SelectionController::arrayModeChanged, this, [this] {
            synchronize(true);
        });
        connect(selection, &SelectionController::coordinatesRebased, this,
                [this](double, double row) {
                    // The ruler has its own physical occurrences, independent of scene rebase.
                    m_lastPhysical -= row;
                });
    }
    if (motion) {
        connect(motion->clock(), &FrameClock::frame, this, [this](double, double dt) {
            advance(dt);
        });
        connect(motion, &MotionController::reducedMotionChanged, this, [this] {
            render(0, m_motion->reducedMotion());
        });
    }
    synchronize(true);
    if (m_slotStates.isEmpty())
        publish();
}
void RulerController::synchronize(bool groupChanged)
{
    if (!m_selection)
        return;
    const auto &pool = m_selection->pool();
    const int lane = ArchivePool::wrap(m_selection->cell().lane, pool.columnCount());
    const int count = pool.rowCount(m_selection->cell().lane);
    const auto location = pool.location(m_selection->albumId());
    const double physical = m_selection->cell().row - 12;
    const bool changedGroup = groupChanged || lane != m_lane;
    const double occurrence = changedGroup ? (location ? location->row - 12 : 0)
                                           : m_selected + physical - m_lastPhysical;
    m_lastPhysical = physical;
    m_lane = lane;
    configure(count, location ? int(location->row - 12) : 0, occurrence, !m_selection->realistic(),
              changedGroup);
}
void RulerController::configure(int count, int row, double physicalRow, bool loop,
                                bool groupChanged)
{
    if (count < 0 || !std::isfinite(physicalRow))
        return;
    const bool first = !m_populated;
    groupChanged |= count != m_count || loop != m_loop;
    m_count = count;
    m_loop = loop;
    // Physical row is used only for overflow. Short columns have one occurrence per album.
    m_selected = loop && count > 12 ? physicalRow : row;
    if (groupChanged || first) {
        m_selected = row;
        regroup(first || (m_motion && m_motion->reducedMotion()));
    }
    m_target = selectionScroll();
    m_populated |= count > 0;
    const auto old = m_drawing;
    render(0, first || (m_motion && m_motion->reducedMotion()));
    if (old == m_drawing)
        publish();
}
bool RulerController::valid(double ordinal) const
{
    return m_count > 0 && ((m_loop && m_count > 12) || (ordinal >= 0 && ordinal < m_count));
}
double RulerController::selectionScroll() const
{
    if (m_count <= 12)
        return 0;
    return m_loop ? m_selected - 5 : std::clamp(m_selected - 5, 0.0, double(m_count - 12));
}
void RulerController::regroup(bool instant)
{
    const double step = m_drawing.tick + m_drawing.gap;
    m_target = selectionScroll();
    m_scroll.finish(m_target);
    const double start = m_count > 12 ? std::floor(m_scroll.value) - 2 : 0;
    std::array<double, 16> destinations;
    std::array<bool, 16> available;
    available.fill(true);
    for (int i = 0; i < 16; ++i)
        destinations[i] = start + i;
    std::stable_sort(destinations.begin(), destinations.end(), [&](double a, double b) {
        const bool av = valid(a) && a - m_scroll.value >= 0 && a - m_scroll.value < 12;
        const bool bv = valid(b) && b - m_scroll.value >= 0 && b - m_scroll.value < 12;
        return av != bv ? av > bv : a < b;
    });
    for (double ordinal : destinations) {
        const double x = (ordinal - m_scroll.value) * step;
        int nearest = -1;
        for (int i = 0; i < 16; ++i)
            if (available[i]
                && (nearest < 0 || std::abs(m_slots[i].x - x) < std::abs(m_slots[nearest].x - x)))
                nearest = i;
        available[nearest] = false;
        auto &slot = m_slots[nearest];
        const bool wasVisible = slot.presence.value > .025 && slot.x + m_drawing.tick > .5
                && slot.x < m_width.value - .5;
        slot.ordinal = ordinal;
        slot.offset.value = instant || !wasVisible ? 0 : slot.x - x;
        if (instant || !wasVisible)
            slot.offset.velocity = 0;
        if (!wasVisible) {
            slot.presence.finish(0);
            slot.height.finish(m_narrow ? 8 : 10);
            slot.opacity.finish(.28);
        }
        const double delay = instant ? 0 : std::clamp(ordinal - m_scroll.value, 0.0, 11.0) * .024;
        const bool changesPresence =
                valid(ordinal) ? slot.presence.value < .025 : slot.presence.value > .025;
        slot.revealAt = m_time + (changesPresence ? delay : 0);
        slot.rippleAt = instant ? -1e20 : m_time + delay;
        slot.pointer = false;
    }
}
void RulerController::recycle()
{
    if (m_count <= 12)
        return;
    const double start = std::floor(m_scroll.value) - 2;
    for (int n = 0; n < 16; ++n) {
        const double ordinal = start + n;
        const bool present = std::any_of(m_slots.begin(), m_slots.end(), [ordinal](const Slot &s) {
            return s.ordinal == ordinal;
        });
        if (present)
            continue;
        auto free = std::find_if(m_slots.begin(), m_slots.end(), [&](const Slot &s) {
            if (s.ordinal >= start && s.ordinal < start + 16)
                return false;
            const double x = (s.ordinal - m_scroll.value) * (m_drawing.tick + m_drawing.gap)
                    + s.offset.value;
            return s.presence.value <= .025 || x + m_drawing.tick <= .5 || x >= m_width.value - .5;
        });
        if (free == m_slots.end())
            break;
        free->ordinal = ordinal;
        free->offset.finish(0);
        free->presence.finish(1);
        free->height.finish(ordinal == m_selected ? m_drawing.height : m_narrow ? 8 : 10);
        free->opacity.finish(ordinal == m_selected ? 1 : .28);
        free->revealAt = 0;
        free->rippleAt = -1e20;
        free->pointer = free->focused = false;
    }
}
void RulerController::render(double dt, bool instant)
{
    const auto old = m_drawing;
    bool moving = approach(m_scroll, m_target, dt, instant);
    moving = approach(m_width,
                      std::max(0.0,
                               std::min(12, m_count) * (m_drawing.tick + m_drawing.gap)
                                       - m_drawing.gap),
                      dt, instant)
            || moving;
    recycle();
    m_drawing.width = std::max(0.0, m_width.value);
    m_drawing.overflow = m_count > 12;
    for (int i = 0; i < 16; ++i) {
        auto &s = m_slots[i];
        auto &mark = m_drawing.marks[i];
        if (instant) {
            s.revealAt = m_time;
            s.rippleAt = -1e20;
        }
        const bool revealed = m_time >= s.revealAt;
        moving = approach(s.presence, valid(s.ordinal) ? double(revealed) : double(!revealed), dt,
                          instant)
                || moving;
        moving = approach(s.offset, 0, dt, instant) || moving;
        s.x = (s.ordinal - m_scroll.value) * (m_drawing.tick + m_drawing.gap) + s.offset.value;
        mark.visible = valid(s.ordinal) && (instant || revealed) && s.presence.value > .025
                && s.x + m_drawing.tick > .5 && s.x < m_width.value - .5;
        mark.active = valid(s.ordinal) && s.ordinal == m_selected;
        mark.hovering = mark.visible && (s.pointer || s.focused);
        const double progress = (m_time - s.rippleAt) / .560;
        const bool rippling = !instant && valid(s.ordinal) && progress >= 0 && progress < 1;
        const double contraction = rippling ? .4 * std::sin(std::acos(-1.0) * progress) : 0;
        const double height = mark.active ? m_drawing.height
                : mark.hovering           ? m_narrow ? 16 : 22
                                          : (m_narrow ? 8 : 10) * (1 - contraction);
        moving = approach(s.height, height, dt, instant) || moving;
        moving = approach(s.opacity, mark.active || mark.hovering ? 1 : .28 - contraction * .2, dt,
                          instant)
                || moving;
        moving |= !instant && (!revealed || (valid(s.ordinal) && m_time < s.rippleAt + .560));
        mark.x = s.x;
        mark.height = std::max(0.0, s.height.value);
        mark.alpha = std::clamp(s.presence.value, 0.0, 1.0) * std::clamp(s.opacity.value, 0.0, 1.0);
    }
    m_moving = moving;
    if (old != m_drawing)
        publish();
}
void RulerController::advance(double motionDt)
{
    if (!m_active || !m_moving || !std::isfinite(motionDt) || motionDt < 0)
        return;
    m_time += motionDt;
    render(motionDt, m_motion && m_motion->reducedMotion());
}
const RulerDrawing &RulerController::drawing() const
{
    return m_drawing;
}
double RulerController::width() const
{
    return m_drawing.width;
}
bool RulerController::narrow() const
{
    return m_narrow;
}
void RulerController::setNarrow(bool narrow)
{
    if (m_narrow == narrow)
        return;
    const double ratio = narrow ? 8.0 / 11 : 11.0 / 8;
    m_narrow = narrow;
    m_drawing.tick = narrow ? 2 : 3;
    m_drawing.gap = narrow ? 6 : 8;
    m_drawing.height = narrow ? 21 : 30;
    m_width.value *= ratio;
    m_width.velocity *= ratio;
    for (auto &s : m_slots) {
        s.x *= ratio;
        s.offset.value *= ratio;
        s.offset.velocity *= ratio;
    }
    render(0, m_motion && m_motion->reducedMotion());
    emit geometryChanged();
}
bool RulerController::active() const
{
    return m_active;
}
void RulerController::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    emit activeChanged();
    publish();
}
QString RulerController::title(double ordinal) const
{
    if (!m_selection || !valid(ordinal) || m_lane < 0
        || m_lane >= m_selection->pool().columnCount())
        return { };
    const auto &ids = m_selection->pool().columns()[m_lane].albumIds;
    return m_selection->coverSource(ids[ArchivePool::wrap(ordinal, m_count)]).second;
}
QVariantList RulerController::slotStates() const
{
    return m_slotStates;
}
void RulerController::publish()
{
    QVariantList result;
    result.reserve(16);
    for (int i = 0; i < 16; ++i) {
        const auto &mark = m_drawing.marks[i];
        result.append(QVariantMap{ { "x", mark.x },
                                   { "enabled", m_active && mark.visible },
                                   { "active", mark.active },
                                   { "title", title(m_slots[i].ordinal) },
                                   { "ordinal", m_slots[i].ordinal },
                                   { "height", mark.height },
                                   { "alpha", mark.alpha },
                                   { "hovering", mark.hovering } });
    }
    m_slotStates = std::move(result);
    emit changed();
}
bool RulerController::click(int slot)
{
    if (!m_active || !m_selection || slot < 0 || slot >= 16 || !m_drawing.marks[slot].visible)
        return false;
    emit inputReceived();
    const double ordinal = m_slots[slot].ordinal;
    if (m_router)
        return m_router->selectRow(m_selection->realistic()
                                           ? ordinal + 12
                                           : m_selection->cell().row + ordinal - m_selected);
    return m_selection->realistic()
            ? m_selection->navigateRow(ordinal + 12)
            : m_selection->navigateRow(m_selection->cell().row + ordinal - m_selected);
}
void RulerController::setInputRouter(InputRouter *router)
{
    m_router = router;
}
void RulerController::hover(int slot, bool pointer, bool focused)
{
    if (slot < 0 || slot >= 16)
        return;
    auto &s = m_slots[slot];
    if (s.pointer == pointer && s.focused == focused)
        return;
    s.pointer = pointer;
    s.focused = focused;
    render(0, m_motion && m_motion->reducedMotion());
}
QVariantMap RulerController::state() const
{
    return { { "scroll", m_scroll.value }, { "scrollVelocity", m_scroll.velocity },
             { "width", width() },         { "selected", m_selected },
             { "time", m_time },           { "slots", slotStates() } };
}
}
