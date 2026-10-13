#pragma once
#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>
#include <array>
#include <cmath>
namespace yalahoo {
struct SceneVector
{
    double x = 0, y = 0, z = 0;
    bool operator==(const SceneVector &) const = default;
    SceneVector operator+(SceneVector b) const
    {
        return { x + b.x, y + b.y, z + b.z };
    }
    SceneVector operator-(SceneVector b) const
    {
        return { x - b.x, y - b.y, z - b.z };
    }
    SceneVector operator*(double s) const
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
inline double dot(SceneVector a, SceneVector b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline SceneVector cross(SceneVector a, SceneVector b)
{
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
struct CameraPose
{
    SceneVector position, aim;
    double span = 7.33;
};
// Immutable value snapshot: future renderers copy this at synchronization.
struct SceneFrame
{
    quint64 revision = 0;
    quint64 inputRevision = 0;
    qint64 inputReceivedNs = 0;
    CameraPose camera;
    SceneVector casePosition;
    QQuaternion cameraRotation;
    QMatrix4x4 view, projection, caseTransform;
    double width = 960, height = 640, fieldOfView = 0;
    double nearPlane = 5, farPlane = 300;
    bool portrait = false;
};
}
