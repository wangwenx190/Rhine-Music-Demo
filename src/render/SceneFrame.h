// Defines the value snapshot consumed by rendering and hit testing. Explicit scene vectors
// preserve source-space precision before conversion at the Qt boundary.

#pragma once
#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>
#include <array>
#include <cmath>
#include "ArchivePool.h"
namespace yalahoo {
namespace sceneDefaults {
// Default source camera and logical frame bounds before the first viewport update.
inline constexpr auto BrowseSpan = 7.33;
inline constexpr auto WidthPixels = 960;
inline constexpr auto HeightPixels = 640;
inline constexpr auto NearPlane = 5;
inline constexpr auto FarPlane = 300;
}

struct SceneVector final
{
    double x = 0, y = 0, z = 0;
    bool operator==(const SceneVector &) const = default;
    constexpr SceneVector operator+(SceneVector b) const
    {
        return { x + b.x, y + b.y, z + b.z };
    }
    constexpr SceneVector operator-(SceneVector b) const
    {
        return { x - b.x, y - b.y, z - b.z };
    }
    constexpr SceneVector operator*(double s) const
    {
        return { x * s, y * s, z * s };
    }
    double length() const
    {
        return std::sqrt(x * x + y * y + z * z);
    }
    SceneVector normalized() const
    {
        return *this * (1 / length());
    }
    QVector3D qt() const
    {
        return { float(x), float(y), float(z) };
    }
};
inline constexpr double dot(SceneVector a, SceneVector b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline constexpr SceneVector cross(SceneVector a, SceneVector b)
{
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
struct CameraPose final
{
    SceneVector position, aim;
    double span = sceneDefaults::BrowseSpan;
};
// Immutable value snapshot: future renderers copy this at synchronization.
struct SceneFrame final
{
    quint64 revision = 0;
    quint64 inputRevision = 0;
    qint64 inputReceivedNs = 0;
    quint64 navigationRevision = 0, navigationInputRevision = 0;
    qint64 navigationCommittedNs = 0;
    CameraPose camera;
    SceneVector casePosition;
    QQuaternion cameraRotation;
    QMatrix4x4 view, projection, caseTransform;
    double width = sceneDefaults::WidthPixels, height = sceneDefaults::HeightPixels,
           fieldOfView = 0;
    double nearPlane = sceneDefaults::NearPlane, farPlane = sceneDefaults::FarPlane;
    double fogNear = 0, fogFar = 0;
    bool portrait = false;
    bool hasArchivePool = false;
    bool individualMaterials = false;
    QVector<ArchiveInstance> instances;
};
}
