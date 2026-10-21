#include "CameraFraming.h"
#include "Units.h"
#include "SceneFrame.h"
#include "SceneGeometry.h"
#include "RowLightController.h"
#include "FrameClock.h"
#include "SelectionController.h"
#include "SceneAdapter.h"
#include "OverviewController.h"
#include "opening/OpeningFrame.h"
#include "MotionFunctions.h"
#include <QVector4D>
#include <algorithm>
#include <numbers>
#include <limits>
#include <chrono>

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
// Camera framing follows source-space layout; projection converts to Qt only at the matrix boundary.
constexpr auto DetailSpan = 5.9;
constexpr auto PortraitMinimumDetailSpan = 6.3;
constexpr auto PortraitCaseSpan = 3.7;
constexpr auto MinimumPortraitContentHeight = 100.0;
constexpr auto PortraitDetailHeightRatio = .54;
constexpr auto PortraitDetailChromePixels = 156;
constexpr auto CameraFollowRate = 9;
constexpr auto BrowseYawDegrees = 59;
constexpr auto DetailYawDegrees = 8;
constexpr auto BrowseElevationDegrees = 25;
constexpr auto BrowseFogFarOffset = 25;
constexpr auto DetailElevationDegrees = 20;
constexpr auto BrowseAimX = 1.091;
constexpr auto BrowseAimY = .045;
constexpr auto BrowseAimZ = .481;
constexpr auto PositionSettledTolerance = .035;
constexpr auto AimVelocityTolerance = .035;
constexpr auto SpanVelocityTolerance = .035;
constexpr auto PointerParallaxDistance = .12;
constexpr auto LandscapeDetailX = .25;
constexpr auto PortraitDetailY = .27;
constexpr auto PortraitDetailTopPixels = 34;
constexpr auto CompactDetailY = .49;
constexpr auto DesktopDetailYPixels = 560.0;
constexpr auto BrowseCameraDistance = 140;
constexpr auto DetailCameraDistance = 72;
constexpr auto LaneOrbitSpeedScale = 2.5;
constexpr auto LaneOrbitAmplitude = .018;
constexpr auto RowOrbitSpeedScale = 3;
constexpr auto RowOrbitAmplitude = .006;
constexpr auto AimSettledTolerance = .006;
constexpr auto OrbitFollowRate = 6;
constexpr auto SpanSettledTolerance = .008;
constexpr auto PositionVelocityTolerance = .09;
constexpr auto BrowseFogNearOffset = 5;
constexpr auto DetailFogFarOffset = 12;
constexpr auto MinimumFovSpan = .1;

constexpr double radians = std::numbers::pi / yalahoo::units::DegreesPerHalfTurn;
std::array<double, yalahoo::units::VectorAxes> components(SceneVector v)
{
    return { v.x, v.y, v.z };
}
SceneVector values(const std::array<ScalarSpring, yalahoo::units::VectorAxes> &s)
{
    return { s[0].value, s[1].value, s[2].value };
}
SceneVector velocities(const std::array<ScalarSpring, yalahoo::units::VectorAxes> &s)
{
    return { s[0].velocity, s[1].velocity, s[2].velocity };
}
}
SelectionController *SceneAdapter::selection() const
{
    return m_selection;
}
void SceneAdapter::setSelection(SelectionController *selection)
{
    if (m_selection == selection)
        return;
    if (m_selection)
        disconnect(m_selection, nullptr, this, nullptr);
    m_selection = selection;
    if (!selection)
        m_explicitLightingInput = false;
    m_selectionRevision = 0;
    if (selection) {
        connect(selection, &SelectionController::frameChanged, this, &SceneAdapter::publish);
        connect(selection, &SelectionController::libraryChanged, this, [this] {
            m_managedDetail = false;
            m_detail = 0;
            snapToTarget();
            Q_EMIT detailChanged();
        });
        connect(selection, &SelectionController::arrayModeChanged, this, [this] {
            m_lighting.ribbon.reset();
        });
        connect(selection, &SelectionController::coordinatesRebased, this,
                [this](double lane, double row) {
                    m_lighting.ribbon.row.translate(-row);
                    m_lighting.ribbon.lane.translate(-lane);
                });
        connect(selection, &QObject::destroyed, this, [this] {
            m_explicitLightingInput = false;
            m_selectionRevision = 0;
            publish();
            Q_EMIT selectionControllerChanged();
        });
    }
    publish();
    Q_EMIT selectionControllerChanged();
}
void SceneAdapter::recordInput()
{
    ++m_frame.inputRevision;
    m_frame.inputReceivedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                      std::chrono::steady_clock::now().time_since_epoch())
                                      .count();
    Q_EMIT frameChanged();
}
void SceneAdapter::recordNavigationCommit(quint64 revision)
{
    m_frame.navigationRevision = revision;
    m_frame.navigationInputRevision = m_frame.inputRevision;
    m_frame.navigationCommittedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                            std::chrono::steady_clock::now().time_since_epoch())
                                            .count();
    Q_EMIT frameChanged();
}
SceneAdapter::SceneAdapter(FrameClock *clock, QObject *parent) : QObject(parent)
{
    // The first responsive frame starts at its target. Opening can observe its authored pose.
    snapToTarget();
    updateLighting(0);
    connect(&m_layout, &LayoutMetrics::changed, this, [this] {
        if (m_openingTime)
            setOpeningTime(m_openingTime);
        else if (m_reduced)
            snapToTarget();
        else
            publish();
    });
    if (clock)
        connect(clock, &FrameClock::frame, this, [this](double, double motionDt) {
            if (!m_openingTime) {
                advance(motionDt);
                updateLighting(motionDt);
            }
        });
}
// Derive source-space camera aim and span before blending detail and overview framing.
// Portrait layout changes framing, while the render viewport remains the actual content size.
CameraPose SceneAdapter::target() const
{
    const double w = m_layout.width(), h = m_layout.height(), aspect = w / h, d = m_detail;
    const double baseSpan = yalahoo::sceneDefaults::BrowseSpan
            + (DetailSpan - yalahoo::sceneDefaults::BrowseSpan) * d;
    const double portraitDetailSpan = std::max(
            PortraitMinimumDetailSpan / aspect,
            PortraitCaseSpan * h
                    / std::max(MinimumPortraitContentHeight,
                               PortraitDetailHeightRatio * h - PortraitDetailChromePixels));
    const double span = m_layout.portrait()
            ? std::max(baseSpan,
                       yalahoo::cameraFraming::PortraitPreviewSpan / aspect
                               + (portraitDetailSpan
                                  - yalahoo::cameraFraming::PortraitPreviewSpan / aspect)
                                       * d)
            : std::max(baseSpan,
                       baseSpan
                               * (yalahoo::cameraFraming::StandardAspectWidth
                                  / yalahoo::cameraFraming::AspectHeightUnits)
                               / aspect);
    const double yaw =
            (BrowseYawDegrees * radians + m_yaw.value) * (1 - d) + DetailYawDegrees * radians * d;
    const double elevation = (BrowseElevationDegrees * radians + m_elevation.value) * (1 - d)
            + DetailElevationDegrees * radians * d;
    const auto basis = cameraFraming::basis(yaw, elevation);
    const auto &direction = basis.direction, &right = basis.right, &up = basis.up;
    SceneVector aim{ -BrowseAimX, -BrowseAimY, BrowseAimZ };
    aim = cameraFraming::browseAim(aim, basis, span, aspect, m_layout.portrait());
    SceneVector detailAim{
        0,
        -sceneGeometry::FloorDepth + settlingWave(0, sceneGeometry::SettledSceneSeconds)
                + (sceneGeometry::CaseHeight + yalahoo::sceneGeometry::InspectionLiftPadding)
                + sceneGeometry::HingeHeight,
        -sceneGeometry::ArchiveDepth
    };
    const double detailX = m_layout.portrait() ? .5 : LandscapeDetailX;
    const double detailY = m_layout.portrait() ? PortraitDetailY + PortraitDetailTopPixels / h
            : m_layout.compact()
            ? CompactDetailY
            : DesktopDetailYPixels / yalahoo::cameraFraming::ReferenceHeightPixels;
    detailAim = detailAim + right * ((.5 - detailX) * w * span / h) + up * ((detailY - .5) * span);
    aim = aim * (1 - d) + detailAim * d;
    auto pose = OverviewController::framing(
            { aim
                      + direction
                              * (BrowseCameraDistance
                                 + (DetailCameraDistance - BrowseCameraDistance) * d),
              aim, span },
            w, h, m_selection ? m_selection->overviewProgress() : 0);
    auto position = pose.position;
    if (!m_reduced && d == 0) {
        position.x += m_pointerX * PointerParallaxDistance;
        position.y -= m_pointerY * PointerParallaxDistance;
    }
    return { position, pose.aim, pose.span };
}
void SceneAdapter::requestDetail(bool detail)
{
    if (!m_selection)
        return;
    m_managedDetail = true;
    m_selection->requestDetail(detail);
    if (m_reduced)
        advance(0);
}
void SceneAdapter::setDetail(double detail)
{
    if (!std::isfinite(detail))
        return;
    detail = std::clamp(detail, 0.0, 1.0);
    if (detail == m_detail)
        return;
    m_managedDetail = false;
    m_detail = detail;
    if (m_selection)
        m_selection->setDetail(detail);
    if (m_reduced)
        snapToTarget();
    else
        publish();
    Q_EMIT detailChanged();
}
void SceneAdapter::setReducedMotion(bool reduced)
{
    if (reduced == m_reduced)
        return;
    m_reduced = reduced;
    m_lighting.setReducedMotion(reduced);
    if (reduced) {
        m_yaw.finish(0);
        m_elevation.finish(0);
        if (m_managedDetail && m_selection)
            advance(0);
        else
            snapToTarget();
    }
    updateLighting(0);
    Q_EMIT reducedMotionChanged();
}
void SceneAdapter::setNavigation(double laneSpeed, double rowSpeed, bool realColumns)
{
    if (!std::isfinite(laneSpeed) || !std::isfinite(rowSpeed))
        return;
    m_laneSpeed = laneSpeed;
    m_rowSpeed = realColumns ? 0 : rowSpeed;
}
void SceneAdapter::setPointer(double x, double y)
{
    if (!std::isfinite(x) || !std::isfinite(y))
        return;
    m_pointerX = std::clamp(x, -1.0, 1.0);
    m_pointerY = std::clamp(y, -1.0, 1.0);
}
void SceneAdapter::advance(double dt)
{
    if (m_openingTime || !std::isfinite(dt) || dt < 0)
        return;
    if (m_selection) {
        const auto speeds = m_selection->navigationSpeeds();
        setNavigation(speeds[0], speeds[1], m_selection->realistic());
    }
    if (m_managedDetail && m_selection) {
        const double detail = m_selection->detailMotion().value();
        if (m_detail != detail) {
            m_detail = detail;
            Q_EMIT detailChanged();
        }
    }
    if (m_reduced) {
        snapToTarget();
        if (m_managedDetail && m_selection)
            m_selection->updateDetailReadiness(dt, true);
        return;
    }
    m_yaw.update(std::clamp(m_laneSpeed / LaneOrbitSpeedScale, -1.0, 1.0) * LaneOrbitAmplitude
                         * (1 - m_detail),
                 OrbitFollowRate, dt);
    m_elevation.update(std::clamp(m_rowSpeed / RowOrbitSpeedScale, -1.0, 1.0) * RowOrbitAmplitude
                               * (1 - m_detail),
                       OrbitFollowRate, dt);
    const auto goal = target();
    const auto p = components(goal.position), a = components(goal.aim);
    for (int i = 0; i < yalahoo::units::VectorAxes; ++i) {
        m_position[i].update(p[i], CameraFollowRate, dt);
        m_aim[i].update(a[i], CameraFollowRate, dt);
    }
    m_span.update(goal.span, CameraFollowRate, dt);
    publish();
    if (m_managedDetail && m_selection)
        m_selection->updateDetailReadiness(dt, settled());
}
void SceneAdapter::setOpeningTime(std::optional<double> time, double dt)
{
    m_openingTime = time;
    m_managedDetail = false;
    m_detail = 0;
    m_yaw.finish(0);
    m_elevation.finish(0);
    if (time) {
        observe(openingCamera(*time, m_layout.width(), m_layout.height(),
                              m_selection ? m_selection->selectedPosition() : SceneVector{ }),
                dt);
    } else {
        observe(m_frame.camera, 0);
    }
    updateLighting(dt);
}
void SceneAdapter::observe(CameraPose pose, double dt)
{
    // Observe authored poses in the existing browse springs; the final hold clears velocity.
    const auto p = components(pose.position), a = components(pose.aim);
    for (int i = 0; i < yalahoo::units::VectorAxes; ++i) {
        m_position[i].velocity = dt > 0 ? (p[i] - m_position[i].value) / dt : 0;
        m_aim[i].velocity = dt > 0 ? (a[i] - m_aim[i].value) / dt : 0;
        m_position[i].value = p[i];
        m_aim[i].value = a[i];
    }
    m_span.velocity = dt > 0 ? (pose.span - m_span.value) / dt : 0;
    m_span.value = pose.span;
    publish();
}
void SceneAdapter::snapToTarget()
{
    const auto goal = target();
    const auto p = components(goal.position), a = components(goal.aim);
    for (int i = 0; i < yalahoo::units::VectorAxes; ++i) {
        m_position[i].finish(p[i]);
        m_aim[i].finish(a[i]);
    }
    m_span.finish(goal.span);
    publish();
}
bool SceneAdapter::settled() const
{
    const auto goal = target();
    return (values(m_position) - goal.position).length() < PositionSettledTolerance
            && (values(m_aim) - goal.aim).length() < AimSettledTolerance
            && std::abs(m_span.value - goal.span) < SpanSettledTolerance
            && velocities(m_position).length() < PositionVelocityTolerance
            && velocities(m_aim).length() < AimVelocityTolerance
            && std::abs(m_span.velocity) < SpanVelocityTolerance;
}
void SceneAdapter::publish()
{
    const SceneVector position = values(m_position), aim = values(m_aim);
    const double renderedDistance = (position - aim).length();
    const double fogNear = renderedDistance + BrowseFogNearOffset * (1 - m_detail) - m_detail;
    const double fogFar =
            renderedDistance + BrowseFogFarOffset * (1 - m_detail) + DetailFogFarOffset * m_detail;
    const SceneVector caseOrigin = m_selection
            ? m_selection->selectedPosition()
            : SceneVector{
                  0,
                  -sceneGeometry::FloorDepth + settlingWave(0, sceneGeometry::SettledSceneSeconds)
                          + sceneGeometry::PreviewLift
                          + (sceneGeometry::InspectionLift - sceneGeometry::PreviewLift) * m_detail,
                  -sceneGeometry::ArchiveDepth
              };
    if (m_frame.revision && m_frame.width == m_layout.width() && m_frame.height == m_layout.height()
        && m_frame.hasArchivePool == bool(m_selection) && m_frame.camera.position == position
        && m_frame.camera.aim == aim && m_frame.camera.span == m_span.value
        && m_frame.fogNear == fogNear && m_frame.fogFar == fogFar
        && m_frame.casePosition == caseOrigin
        && m_frame.individualMaterials == (m_managedDetail && bool(m_selection))
        && m_selectionRevision == (m_selection ? m_selection->revision() : 0))
        return;
    m_selectionRevision = m_selection ? m_selection->revision() : 0;
    ++m_frame.revision;
    m_frame.width = m_layout.width();
    m_frame.height = m_layout.height();
    m_frame.portrait = m_layout.portrait();
    m_frame.camera = { position, aim, m_span.value };
    m_frame.fogNear = fogNear;
    m_frame.fogFar = fogFar;
    const double distance = std::max(1.0, renderedDistance);
    m_frame.fieldOfView =
            2 * std::atan(std::max(MinimumFovSpan, m_span.value) / (2 * distance)) / radians;
    m_frame.view.setToIdentity();
    m_frame.view.lookAt(cameraPosition(), cameraAim(), { 0, 1, 0 });
    m_frame.cameraRotation =
            QQuaternion::fromRotationMatrix(m_frame.view.inverted().normalMatrix());
    m_frame.projection.setToIdentity();
    m_frame.projection.perspective(float(m_frame.fieldOfView),
                                   float(m_frame.width / m_frame.height), float(clipNear()),
                                   float(clipFar()));
    m_viewProjection = m_frame.projection * m_frame.view;
    m_frame.casePosition = caseOrigin;
    m_frame.caseTransform.setToIdentity();
    m_frame.caseTransform.translate(casePosition());
    m_frame.hasArchivePool = bool(m_selection);
    m_frame.individualMaterials = m_managedDetail && bool(m_selection);
    m_frame.instances = m_selection ? m_selection->instances() : QVector<ArchiveInstance>{ };
    if (m_selection) {
        m_frame.caseTransform.rotate(m_selection->selectedRotation());
        const auto cell = m_selection->cell();
        setLightingInput({ caseOrigin, cell.row, cell.lane, m_selection->realistic() },
                         m_selection->selectedRotation());
    }
    Q_EMIT frameChanged();
}
void SceneAdapter::setLightingInput(RowLightingInput input, QQuaternion rotation)
{
    m_explicitLightingInput = true;
    m_lightingInput = input;
    m_caseRotation = rotation;
}
void SceneAdapter::updateLighting(double dt)
{
    auto input = m_lightingInput;
    if (!m_explicitLightingInput) {
        input.origin = m_frame.casePosition;
        input.row = input.origin.z / sceneGeometry::RowSpacing;
        input.lane = input.origin.x / sceneGeometry::ColumnSpacing;
    }
    input.reduced = m_reduced || input.reduced;
    input.cinematic = m_openingTime.has_value();
    input.introTime = m_openingTime;
    const auto localAim =
            m_caseRotation.rotatedVector({ -float(yalahoo::cameraFraming::LocalLightAimX),
                                           float(yalahoo::cameraFraming::LocalLightAimY), 0 });
    const auto offset = cameraRotation().rotatedVector({ -yalahoo::cameraFraming::LightOffsetX,
                                                         -yalahoo::cameraFraming::LightOffsetY,
                                                         yalahoo::cameraFraming::LightOffsetZ });
    m_lighting.update(input, input.origin + SceneVector{ localAim.x(), localAim.y(), localAim.z() },
                      { offset.x(), offset.y(), offset.z() }, m_detail, dt);
}
double SceneAdapter::focalDepth(QVector3D origin) const
{
    // Frozen scene.ts: (model.position + (0,2,0)) in camera view space.
    return -(m_frame.view * QVector4D(origin + QVector3D(0, 2, 0), 1)).z();
}
QVector3D SceneAdapter::projectWorldPoint(QVector3D world) const
{
    const auto clip = m_viewProjection * QVector4D(world, 1);
    if (clip.w() <= 0)
        return { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
                 -1 };
    const auto ndc = clip.toVector3DAffine();
    return { float((ndc.x() + 1) * .5 * m_frame.width), float((1 - ndc.y()) * .5 * m_frame.height),
             ndc.z() };
}
QVariantMap SceneAdapter::hitTest(double x, double y) const
{
    if (m_selection)
        return m_selection->hitTest(m_frame, x, y);
    if (!std::isfinite(x) || !std::isfinite(y) || x < 0 || y < 0 || x >= m_frame.width
        || y >= m_frame.height)
        return { };
    bool invertible = false;
    const auto inverse =
            (m_frame.projection * m_frame.view * m_frame.caseTransform).inverted(&invertible);
    if (!invertible)
        return { };
    const double nx = 2 * x / m_frame.width - 1, ny = 1 - 2 * y / m_frame.height;
    const auto start = (inverse * QVector4D(float(nx), float(ny), -1, 1)).toVector3DAffine();
    const auto end = (inverse * QVector4D(float(nx), float(ny), 1, 1)).toVector3DAffine();
    const auto delta = end - start;
    double enter = 0, exit = 1;
    const std::array<double, yalahoo::units::VectorAxes> lo{ -sceneGeometry::CaseHalfWidth,
                                                             sceneGeometry::CaseBottom,
                                                             -sceneGeometry::CaseHalfDepth },
            hi{ sceneGeometry::CaseHalfWidth, sceneGeometry::CaseTop,
                sceneGeometry::CaseHalfDepth };
    for (int i = 0; i < yalahoo::units::VectorAxes; ++i) {
        if (std::abs(delta[i]) < yalahoo::sceneGeometry::ParallelRayTolerance) {
            if (start[i] < lo[i] || start[i] > hi[i])
                return { };
        } else {
            double a = (lo[i] - start[i]) / delta[i], b = (hi[i] - start[i]) / delta[i];
            if (a > b)
                std::swap(a, b);
            enter = std::max(enter, a);
            exit = std::min(exit, b);
            if (enter > exit)
                return { };
        }
    }
    const auto local = start + delta * float(enter);
    return { { u"objectId"_s, u"inspection-case"_s },
             { u"localPoint"_s, QVariant::fromValue(local) },
             { u"worldPoint"_s, QVariant::fromValue(m_frame.caseTransform.map(local)) },
             { u"frameRevision"_s, QVariant::fromValue(frameRevision()) } };
}
}
