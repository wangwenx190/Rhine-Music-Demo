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
#include <limits>
#include <optional>

namespace yalahoo {
namespace caseBounds {
inline constexpr std::array<double, units::VectorAxes> Low{ -sceneGeometry::CaseHalfWidth,
                                                            sceneGeometry::CaseBottom,
                                                            -sceneGeometry::CaseHalfDepth };
inline constexpr std::array<double, units::VectorAxes> High{ sceneGeometry::CaseHalfWidth,
                                                             sceneGeometry::CaseTop,
                                                             sceneGeometry::CaseHalfDepth };
}
inline bool caseIntersectsClip(const QMatrix4x4 &matrix)
{
    // Maximize w +/- axis over the local AABB instead of transforming eight corners.
    // Double evaluation and a float roundoff allowance keep boundary boxes conservative
    // relative to the shader's float matrix/vector arithmetic, including folded cases.
    constexpr auto RoundoffAllowance = 8 * std::numeric_limits<float>::epsilon();
    for (int axis = 0; axis < units::VectorAxes; ++axis) {
        for (const double sign : { -1.0, 1.0 }) {
            double maximum = double(matrix(3, 3)) + sign * matrix(axis, 3);
            double magnitude = std::abs(double(matrix(3, 3))) + std::abs(double(matrix(axis, 3)));
            for (int component = 0; component < units::VectorAxes; ++component) {
                const double w = matrix(3, component), a = matrix(axis, component);
                const double coefficient = w + sign * a;
                maximum += coefficient
                        * (coefficient >= 0 ? caseBounds::High[component]
                                            : caseBounds::Low[component]);
                magnitude += (std::abs(w) + std::abs(a))
                        * std::max(std::abs(caseBounds::Low[component]),
                                   std::abs(caseBounds::High[component]));
            }
            if (maximum < -RoundoffAllowance * magnitude)
                return false;
        }
    }
    return true;
}
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
    double enter = 0, exit = 1;
    for (int axis = 0; axis < units::VectorAxes; ++axis) {
        if (!std::isfinite(start[axis]) || !std::isfinite(delta[axis]))
            return std::nullopt;
        if (std::abs(delta[axis]) < sceneGeometry::ParallelRayTolerance) {
            if (start[axis] < caseBounds::Low[axis] || start[axis] > caseBounds::High[axis])
                return std::nullopt;
            continue;
        }
        double a = (caseBounds::Low[axis] - start[axis]) / delta[axis];
        double b = (caseBounds::High[axis] - start[axis]) / delta[axis];
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
