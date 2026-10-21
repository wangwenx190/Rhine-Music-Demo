// Constructs Qt splines and samples CSS cubic curves with the source endpoint convention.

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

// Bounded bisection retains the caller's precision budget for analytical source comparisons.
inline double sampleCubicBezier(double x, QPointF first, QPointF second, int searchSteps)
{
    const auto cubic = [](double t, double a, double b) {
        const double u = 1 - t;
        return 3 * u * u * t * a + 3 * u * t * t * b + t * t * t;
    };
    double low = 0, high = 1;
    for (int i = 0; i < searchSteps; ++i) {
        const double t = (low + high) / 2;
        if (cubic(t, first.x(), second.x()) < x)
            low = t;
        else
            high = t;
    }
    return cubic((low + high) / 2, first.y(), second.y());
}
}
