// Constructs Qt cubic splines with the source CSS endpoint convention.

#pragma once
#include <QEasingCurve>
#include <QPointF>

namespace yalahoo {
inline QEasingCurve cubicBezier(double x1, double y1, double x2, double y2)
{
    QEasingCurve curve(QEasingCurve::BezierSpline);
    curve.addCubicBezierSegment({ x1, y1 }, { x2, y2 }, { 1, 1 });
    return curve;
}
}
