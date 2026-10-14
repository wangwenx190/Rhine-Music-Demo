#include "PointerDragController.h"
#include "MotionFunctions.h"
#include <algorithm>
#include <cmath>

namespace yalahoo {
namespace {
bool finite(QPointF p)
{
    return std::isfinite(p.x()) && std::isfinite(p.y());
}
}
bool PointerDragController::press(QPointF point, int pointerId)
{
    if (!finite(point) || pointerId < 0)
        return false;
    if (dragging()) {
        cancel();
        return false;
    }
    m_pointer = pointerId;
    m_start = m_previous = point;
    m_moved = false;
    return true;
}
bool PointerDragController::move(QPointF point, int pointerId)
{
    if (!finite(point) || pointerId != m_pointer || !dragging())
        return false;
    m_target = std::clamp(m_target + (point.x() - m_previous.x()) * .004, -.8, .8);
    m_previous = point;
    m_moved |= std::hypot(point.x() - m_start.x(), point.y() - m_start.y()) > 6;
    return true;
}
bool PointerDragController::release(QPointF point, int pointerId)
{
    if (pointerId != m_pointer || !dragging())
        return false;
    const bool click = finite(point) && !m_moved
            && std::hypot(point.x() - m_start.x(), point.y() - m_start.y()) <= 6;
    cancel();
    return click;
}
void PointerDragController::cancel()
{
    m_pointer = -1;
}
void PointerDragController::reset()
{
    cancel();
    m_target = 0;
}
double PointerDragController::advance(double rotation, double dt, bool inspecting,
                                      bool reduced) const
{
    if (!std::isfinite(dt) || dt < 0)
        return rotation;
    if (reduced)
        return inspecting ? m_target : 0;
    return inspecting ? rotation + (m_target - rotation) * (1 - std::exp(-dt * 2.8))
                      : returnStep(rotation, dt, false);
}
}
