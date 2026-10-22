// Shares viewport projection and segment/case intersection in source coordinates.
// Picking callers own matrix inversion and transform the segment into each case's local space.

#pragma once
#include "SceneGeometry.h"
#include "Units.h"
#include <QMatrix4x4>
#include <QSizeF>
#include <QVector3D>
#include <QVector4D>
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace yalahoo {
inline std::optional<QVector3D> projectToViewport(const QMatrix4x4 &viewProjection, QVector3D world,
                                                  QSizeF viewport)
{
    const auto clip = viewProjection * QVector4D(world, 1);
    if (!std::isfinite(clip.x()) || !std::isfinite(clip.y()) || !std::isfinite(clip.z())
        || !std::isfinite(clip.w()) || clip.w() <= 0 || !std::isfinite(viewport.width())
        || !std::isfinite(viewport.height()) || viewport.width() <= 0 || viewport.height() <= 0)
        return std::nullopt;
    const auto ndc = clip.toVector3DAffine();
    const QVector3D point(float((ndc.x() + 1) * .5 * viewport.width()),
                          float((1 - ndc.y()) * .5 * viewport.height()), ndc.z());
    if (!std::isfinite(point.x()) || !std::isfinite(point.y()) || !std::isfinite(point.z()))
        return std::nullopt;
    return point;
}
inline std::optional<double> caseRayEntry(QVector3D start, QVector3D delta)
{
    constexpr std::array<double, units::VectorAxes> low{ -sceneGeometry::CaseHalfWidth,
                                                         sceneGeometry::CaseBottom,
                                                         -sceneGeometry::CaseHalfDepth };
    constexpr std::array<double, units::VectorAxes> high{ sceneGeometry::CaseHalfWidth,
                                                          sceneGeometry::CaseTop,
                                                          sceneGeometry::CaseHalfDepth };
    double enter = 0, exit = 1;
    for (int axis = 0; axis < units::VectorAxes; ++axis) {
        if (!std::isfinite(start[axis]) || !std::isfinite(delta[axis]))
            return std::nullopt;
        if (std::abs(delta[axis]) < sceneGeometry::ParallelRayTolerance) {
            if (start[axis] < low[axis] || start[axis] > high[axis])
                return std::nullopt;
            continue;
        }
        double a = (low[axis] - start[axis]) / delta[axis];
        double b = (high[axis] - start[axis]) / delta[axis];
        if (a > b)
            std::swap(a, b);
        enter = std::max(enter, a);
        exit = std::min(exit, b);
        if (enter > exit)
            return std::nullopt;
    }
    return enter;
}
}
