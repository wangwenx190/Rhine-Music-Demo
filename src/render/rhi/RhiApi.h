// Wraps low-level RHI resource validation and shader loading. Failure is reported before
// partially initialized native resources can be submitted.

#pragma once
// All limited-compatibility APIs are isolated here and in this directory.
#include <rhi/qrhi.h>
#include <rhi/qshader.h>
#include <QFile>
#include <stdexcept>
#include <memory>
namespace yalahoo::rhi {
namespace shadowCamera {
// Source directional-light projection is independent of the viewer camera.
inline constexpr auto PositionXMagnitude = 6;
inline constexpr auto PositionY = 14;
inline constexpr auto PositionZMagnitude = 5;
inline constexpr auto HalfWidth = 16;
inline constexpr auto HalfHeight = 15;
inline constexpr auto NearPlane = .1f;
inline constexpr auto FarPlane = 45;
}

inline void require(bool ok, const char *reason)
{
    if (!ok)
        throw std::runtime_error(reason);
}
struct ResourceUpdateDeleter final
{
    void operator()(QRhiResourceUpdateBatch *batch) const noexcept
    {
        batch->release();
    }
};
using ResourceUpdates = std::unique_ptr<QRhiResourceUpdateBatch, ResourceUpdateDeleter>;
QShader shader(const QString &path);
inline const QMatrix4x4 &lightView()
{
    static const auto view = [] {
        QMatrix4x4 m;
        m.lookAt({ -shadowCamera::PositionXMagnitude, shadowCamera::PositionY,
                   -shadowCamera::PositionZMagnitude },
                 { 0, 0, 0 }, { 0, 1, 0 });
        return m;
    }();
    return view;
}
inline const QMatrix4x4 &lightProjection()
{
    static const auto projection = [] {
        QMatrix4x4 m;
        m.ortho(-shadowCamera::HalfWidth, shadowCamera::HalfWidth, -shadowCamera::HalfHeight,
                shadowCamera::HalfHeight, shadowCamera::NearPlane, shadowCamera::FarPlane);
        return m;
    }();
    return projection;
}
inline const QMatrix4x4 &lightViewProjection()
{
    static const auto matrix = lightProjection() * lightView();
    return matrix;
}
}
