#pragma once
#include <QPointF>

namespace yalahoo {
// Pointer samples replace one target. Rotation follows on the sole FrameClock;
// releasing retains the authored target, returning uses motion::returnStep.
class PointerDragController
{
public:
    bool press(QPointF point, int pointerId = 0);
    bool move(QPointF point, int pointerId = 0);
    bool release(QPointF point, int pointerId = 0);
    void cancel();
    void reset();
    bool dragging() const
    {
        return m_pointer >= 0;
    }
    double target() const
    {
        return m_target;
    }
    double advance(double rotation, double dt, bool inspecting, bool reduced) const;

private:
    QPointF m_start, m_previous;
    double m_target = 0;
    int m_pointer = -1;
    bool m_moved = false;
};
}
