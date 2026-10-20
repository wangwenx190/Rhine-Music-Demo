#include "FrameClock.h"
#include "MotionController.h"
#include "SelectionController.h"
#include "RulerController.h"
#include "InputRouter.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
// The fixed physical tick pool includes overscan; presence hysteresis prevents visible recycling.
constexpr auto PositionTolerance = .001;
constexpr auto VelocityTolerance = .01;
constexpr auto FollowRate = 18;
constexpr auto PhysicalTickCount = 16;
constexpr auto NarrowHoverHeightPixels = 16;
constexpr auto TickStridePixels = 11;
constexpr auto RestingHeightPixels = 10;
constexpr auto RestingOpacity = .28;
constexpr auto VisibleTickCount = 12;
constexpr auto FirstAlbumRow = 12;
constexpr auto SelectedLeadingTicks = 5;
constexpr auto VisiblePresenceThreshold = .025;
constexpr auto NarrowRestingHeightPixels = 8;
constexpr auto WideGapPixels = 8;
constexpr auto NarrowTickPixels = 2;
constexpr auto OverscanLeadingTicks = 2;
constexpr auto RippleStaggerSeconds = .024;
constexpr auto InactiveRippleTime = 1e20;
constexpr auto RippleDurationSeconds = .560;
constexpr auto RippleContraction = .4;
constexpr auto HoverHeightPixels = 22;
constexpr auto RippleOpacityLoss = .2;
constexpr auto NarrowStridePixels = 8.0;
constexpr auto WideStridePixels = 11.0;
constexpr auto LastVisibleOrdinalOffset = 11.0;
constexpr auto WideTickPixels = 3;
constexpr auto NarrowGapPixels = 6;
constexpr auto NarrowActiveHeightPixels = 21;
constexpr auto ActiveHeightPixels = 30;
}

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
bool approach(ScalarSpring &s, double target, double dt, bool instant)
{
    if (instant
        || (std::abs(s.value - target) < PositionTolerance
            && std::abs(s.velocity) < VelocityTolerance)) {
        s.finish(target);
        return false;
    }
    s.update(target, FollowRate, dt);
    return true;
}
}
RulerController::RulerController(MotionController *motion, SelectionController *selection,
                                 QObject *parent)
    : QObject(parent), m_motion(motion), m_selection(selection)
{
    for (int i = 0; i < PhysicalTickCount; ++i) {
        m_slots[i].ordinal = i;
        m_slots[i].x = i * TickStridePixels;
        m_slots[i].height.finish(RestingHeightPixels);
        m_slots[i].opacity.finish(RestingOpacity);
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
    const double physical = m_selection->cell().row - FirstAlbumRow;
    const bool changedGroup = groupChanged || lane != m_lane;
    const double occurrence = changedGroup ? (location ? location->row - FirstAlbumRow : 0)
                                           : m_selected + physical - m_lastPhysical;
    m_lastPhysical = physical;
    m_lane = lane;
    configure(count, location ? int(location->row - FirstAlbumRow) : 0, occurrence,
              !m_selection->realistic(), changedGroup);
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
    m_selected = loop && count > VisibleTickCount ? physicalRow : row;
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
    return m_count > 0
            && ((m_loop && m_count > VisibleTickCount) || (ordinal >= 0 && ordinal < m_count));
}
double RulerController::selectionScroll() const
{
    if (m_count <= VisibleTickCount)
        return 0;
    return m_loop ? m_selected - SelectedLeadingTicks
                  : std::clamp(m_selected - SelectedLeadingTicks, 0.0,
                               double(m_count - VisibleTickCount));
}
void RulerController::regroup(bool instant)
{
    const double step = m_drawing.tick + m_drawing.gap;
    m_target = selectionScroll();
    m_scroll.finish(m_target);
    const double start =
            m_count > VisibleTickCount ? std::floor(m_scroll.value) - OverscanLeadingTicks : 0;
    std::array<double, PhysicalTickCount> destinations;
    std::array<bool, PhysicalTickCount> available;
    available.fill(true);
    for (int i = 0; i < PhysicalTickCount; ++i)
        destinations[i] = start + i;
    std::stable_sort(destinations.begin(), destinations.end(), [&](double a, double b) {
        const bool av =
                valid(a) && a - m_scroll.value >= 0 && a - m_scroll.value < VisibleTickCount;
        const bool bv =
                valid(b) && b - m_scroll.value >= 0 && b - m_scroll.value < VisibleTickCount;
        return av != bv ? av > bv : a < b;
    });
    for (double ordinal : destinations) {
        const double x = (ordinal - m_scroll.value) * step;
        int nearest = -1;
        for (int i = 0; i < PhysicalTickCount; ++i)
            if (available[i]
                && (nearest < 0 || std::abs(m_slots[i].x - x) < std::abs(m_slots[nearest].x - x)))
                nearest = i;
        available[nearest] = false;
        auto &slot = m_slots[nearest];
        const bool wasVisible = slot.presence.value > VisiblePresenceThreshold
                && slot.x + m_drawing.tick > .5 && slot.x < m_width.value - .5;
        slot.ordinal = ordinal;
        slot.offset.value = instant || !wasVisible ? 0 : slot.x - x;
        if (instant || !wasVisible)
            slot.offset.velocity = 0;
        if (!wasVisible) {
            slot.presence.finish(0);
            slot.height.finish(m_narrow ? NarrowRestingHeightPixels : RestingHeightPixels);
            slot.opacity.finish(RestingOpacity);
        }
        const double delay = instant
                ? 0
                : std::clamp(ordinal - m_scroll.value, 0.0, LastVisibleOrdinalOffset)
                        * RippleStaggerSeconds;
        const bool changesPresence = valid(ordinal)
                ? slot.presence.value < VisiblePresenceThreshold
                : slot.presence.value > VisiblePresenceThreshold;
        slot.revealAt = m_time + (changesPresence ? delay : 0);
        slot.rippleAt = instant ? -InactiveRippleTime : m_time + delay;
        slot.pointer = false;
    }
}
void RulerController::recycle()
{
    if (m_count <= VisibleTickCount)
        return;
    const double start = std::floor(m_scroll.value) - OverscanLeadingTicks;
    for (int n = 0; n < PhysicalTickCount; ++n) {
        const double ordinal = start + n;
        const bool present = std::any_of(m_slots.begin(), m_slots.end(), [ordinal](const Slot &s) {
            return s.ordinal == ordinal;
        });
        if (present)
            continue;
        auto free = std::find_if(m_slots.begin(), m_slots.end(), [&](const Slot &s) {
            if (s.ordinal >= start && s.ordinal < start + PhysicalTickCount)
                return false;
            const double x = (s.ordinal - m_scroll.value) * (m_drawing.tick + m_drawing.gap)
                    + s.offset.value;
            return s.presence.value <= VisiblePresenceThreshold || x + m_drawing.tick <= .5
                    || x >= m_width.value - .5;
        });
        if (free == m_slots.end())
            break;
        free->ordinal = ordinal;
        free->offset.finish(0);
        free->presence.finish(1);
        free->height.finish(ordinal == m_selected ? m_drawing.height
                                    : m_narrow    ? NarrowRestingHeightPixels
                                                  : RestingHeightPixels);
        free->opacity.finish(ordinal == m_selected ? 1 : RestingOpacity);
        free->revealAt = 0;
        free->rippleAt = -InactiveRippleTime;
        free->pointer = free->focused = false;
    }
}
void RulerController::render(double dt, bool instant)
{
    const auto old = m_drawing;
    bool moving = approach(m_scroll, m_target, dt, instant);
    moving =
            approach(m_width,
                     std::max(0.0,
                              std::min(VisibleTickCount, m_count) * (m_drawing.tick + m_drawing.gap)
                                      - m_drawing.gap),
                     dt, instant)
            || moving;
    recycle();
    m_drawing.width = std::max(0.0, m_width.value);
    m_drawing.overflow = m_count > VisibleTickCount;
    for (int i = 0; i < PhysicalTickCount; ++i) {
        auto &s = m_slots[i];
        auto &mark = m_drawing.marks[i];
        if (instant) {
            s.revealAt = m_time;
            s.rippleAt = -InactiveRippleTime;
        }
        const bool revealed = m_time >= s.revealAt;
        moving = approach(s.presence, valid(s.ordinal) ? double(revealed) : double(!revealed), dt,
                          instant)
                || moving;
        moving = approach(s.offset, 0, dt, instant) || moving;
        s.x = (s.ordinal - m_scroll.value) * (m_drawing.tick + m_drawing.gap) + s.offset.value;
        mark.visible = valid(s.ordinal) && (instant || revealed)
                && s.presence.value > VisiblePresenceThreshold && s.x + m_drawing.tick > .5
                && s.x < m_width.value - .5;
        mark.active = valid(s.ordinal) && s.ordinal == m_selected;
        mark.hovering = mark.visible && (s.pointer || s.focused);
        const double progress = (m_time - s.rippleAt) / RippleDurationSeconds;
        const bool rippling = !instant && valid(s.ordinal) && progress >= 0 && progress < 1;
        const double contraction =
                rippling ? RippleContraction * std::sin(std::acos(-1.0) * progress) : 0;
        const double height = mark.active ? m_drawing.height
                : mark.hovering
                ? m_narrow ? NarrowHoverHeightPixels : HoverHeightPixels
                : (m_narrow ? NarrowRestingHeightPixels : RestingHeightPixels) * (1 - contraction);
        moving = approach(s.height, height, dt, instant) || moving;
        moving = approach(s.opacity,
                          mark.active || mark.hovering
                                  ? 1
                                  : RestingOpacity - contraction * RippleOpacityLoss,
                          dt, instant)
                || moving;
        moving |= !instant
                && (!revealed || (valid(s.ordinal) && m_time < s.rippleAt + RippleDurationSeconds));
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
    const double ratio =
            narrow ? NarrowStridePixels / TickStridePixels : WideStridePixels / NarrowStridePixels;
    m_narrow = narrow;
    m_drawing.tick = narrow ? NarrowTickPixels : WideTickPixels;
    m_drawing.gap = narrow ? NarrowGapPixels : WideGapPixels;
    m_drawing.height = narrow ? NarrowActiveHeightPixels : ActiveHeightPixels;
    m_width.value *= ratio;
    m_width.velocity *= ratio;
    for (auto &s : m_slots) {
        s.x *= ratio;
        s.offset.value *= ratio;
        s.offset.velocity *= ratio;
    }
    render(0, m_motion && m_motion->reducedMotion());
    Q_EMIT geometryChanged();
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
    Q_EMIT activeChanged();
    publish();
}
QString RulerController::title(double ordinal) const
{
    if (!m_selection || !valid(ordinal) || m_lane < 0
        || m_lane >= m_selection->pool().columnCount())
        return { };
    const auto &ids = m_selection->pool().columns()[m_lane].albumIds;
    const int row = ArchivePool::wrap(ordinal, m_count);
    return row < ids.size() ? m_selection->albumTitle(ids[row]) : QString{ };
}
QVariantList RulerController::slotStates() const
{
    return m_slotStates;
}
void RulerController::publish()
{
    QVariantList result;
    result.reserve(PhysicalTickCount);
    for (int i = 0; i < PhysicalTickCount; ++i) {
        const auto &mark = m_drawing.marks[i];
        result.append(QVariantMap{ { u"x"_s, mark.x },
                                   { u"enabled"_s, m_active && mark.visible },
                                   { u"active"_s, mark.active },
                                   { u"title"_s, title(m_slots[i].ordinal) },
                                   { u"ordinal"_s, m_slots[i].ordinal },
                                   { u"height"_s, mark.height },
                                   { u"alpha"_s, mark.alpha },
                                   { u"hovering"_s, mark.hovering } });
    }
    m_slotStates = std::move(result);
    Q_EMIT changed();
}
bool RulerController::click(int slot)
{
    if (!m_active || !m_selection || slot < 0 || slot >= PhysicalTickCount
        || !m_drawing.marks[slot].visible)
        return false;
    Q_EMIT inputReceived();
    const double ordinal = m_slots[slot].ordinal;
    if (m_router)
        return m_router->selectRow(m_selection->realistic()
                                           ? ordinal + FirstAlbumRow
                                           : m_selection->cell().row + ordinal - m_selected);
    return m_selection->realistic()
            ? m_selection->navigateRow(ordinal + FirstAlbumRow)
            : m_selection->navigateRow(m_selection->cell().row + ordinal - m_selected);
}
void RulerController::setInputRouter(InputRouter *router)
{
    m_router = router;
}
void RulerController::hover(int slot, bool pointer, bool focused)
{
    if (slot < 0 || slot >= PhysicalTickCount)
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
    return { { u"scroll"_s, m_scroll.value }, { u"scrollVelocity"_s, m_scroll.velocity },
             { u"width"_s, width() },         { u"selected"_s, m_selected },
             { u"moving"_s, m_moving },       { u"time"_s, m_time },
             { u"slots"_s, slotStates() } };
}
}
