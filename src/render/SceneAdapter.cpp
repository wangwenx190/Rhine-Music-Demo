#include "SceneAdapter.h"
#include <memory>
#include <optional>
#include "LayoutMetrics.h"
#include "ScalarSpring.h"
#include "SelectionLighting.h"
#include "RenderQuality.h"
#include "RenderBackend.h"
#include <QVariantMap>
#include <utility>
#include "SceneMath.h"
#include "CameraFraming.h"
#include "Units.h"
#include "SceneFrame.h"
#include "SceneGeometry.h"
#include "RowLightController.h"
#include "FrameClock.h"
#include "SelectionController.h"
#include "OverviewController.h"
#include "opening/OpeningFrame.h"
#include "MotionFunctions.h"
#include <QVector4D>
#include <algorithm>
#include <numbers>
#include <limits>
#include "MonotonicTime.h"

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
struct SceneAdapter::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    explicit Private(SceneAdapter *owner) : q(owner)
    {
    }
    SceneAdapter *const q;

    void publish();
    QPointer<SelectionController> m_selection;
    quint64 m_selectionRevision = 0;
    LayoutMetrics m_layout;
    SelectionLighting m_lighting;
    RenderQuality m_quality;
    RowLightingInput m_lightingInput;
    QQuaternion m_caseRotation;
    bool m_explicitLightingInput = false;
    SceneFrame m_frame;
    QMatrix4x4 m_viewProjection;
    std::array<ScalarSpring, 3> m_position, m_aim;
    ScalarSpring m_span, m_yaw, m_elevation;
    double m_detail = 0, m_laneSpeed = 0, m_rowSpeed = 0, m_pointerX = 0, m_pointerY = 0;
    bool m_reduced = false, m_managedDetail = false;
    std::optional<double> m_openingTime;
};
SelectionController *SceneAdapter::selection() const
{
    return d->m_selection;
}
void SceneAdapter::setSelection(SelectionController *selection)
{
    if (d->m_selection == selection)
        return;
    if (d->m_selection)
        disconnect(d->m_selection, nullptr, this, nullptr);
    d->m_selection = selection;
    if (!selection)
        d->m_explicitLightingInput = false;
    d->m_selectionRevision = 0;
    if (selection) {
        connect(selection, &SelectionController::frameChanged, this, [this] {
            d->publish();
        });
        connect(selection, &SelectionController::libraryChanged, this, [this] {
            d->m_managedDetail = false;
            d->m_detail = 0;
            snapToTarget();
            Q_EMIT detailChanged();
        });
        connect(selection, &SelectionController::arrayModeChanged, this, [this] {
            d->m_lighting.ribbon.reset();
        });
        connect(selection, &SelectionController::coordinatesRebased, this,
                [this](double lane, double row) {
                    d->m_lighting.ribbon.row.translate(-row);
                    d->m_lighting.ribbon.lane.translate(-lane);
                });
        connect(selection, &QObject::destroyed, this, [this] {
            d->m_explicitLightingInput = false;
            d->m_selectionRevision = 0;
            d->publish();
            Q_EMIT selectionControllerChanged();
        });
    }
    d->publish();
    Q_EMIT selectionControllerChanged();
}
void SceneAdapter::recordInput()
{
    ++d->m_frame.inputRevision;
    d->m_frame.inputReceivedNs = yalahoo::monotonicNanoseconds();
    Q_EMIT frameChanged();
}
void SceneAdapter::recordNavigationCommit(quint64 revision)
{
    d->m_frame.navigationRevision = revision;
    d->m_frame.navigationInputRevision = d->m_frame.inputRevision;
    d->m_frame.navigationCommittedNs = yalahoo::monotonicNanoseconds();
    Q_EMIT frameChanged();
}
SceneAdapter::SceneAdapter(FrameClock *clock, QObject *parent)
    : QObject(parent), d(std::make_unique<Private>(this))
{
    // The first responsive frame starts at its target. Opening can observe its authored pose.
    snapToTarget();
    updateLighting(0);
    connect(&d->m_layout, &LayoutMetrics::changed, this, [this] {
        if (d->m_openingTime)
            setOpeningTime(d->m_openingTime);
        else if (d->m_reduced)
            snapToTarget();
        else
            d->publish();
    });
    if (clock)
        connect(clock, &FrameClock::frame, this, [this](double, double motionDt) {
            if (!d->m_openingTime) {
                advance(motionDt);
                updateLighting(motionDt);
            }
        });
}
// Derive source-space camera aim and span before blending detail and overview framing.
// Portrait layout changes framing, while the render viewport remains the actual content size.
CameraPose SceneAdapter::target() const
{
    const double w = d->m_layout.width(), h = d->m_layout.height(), aspect = w / h,
                 detailProgress = d->m_detail;
    const double baseSpan = yalahoo::sceneDefaults::BrowseSpan
            + (DetailSpan - yalahoo::sceneDefaults::BrowseSpan) * detailProgress;
    const double portraitDetailSpan = std::max(
            PortraitMinimumDetailSpan / aspect,
            PortraitCaseSpan * h
                    / std::max(MinimumPortraitContentHeight,
                               PortraitDetailHeightRatio * h - PortraitDetailChromePixels));
    const double span = d->m_layout.portrait()
            ? std::max(baseSpan,
                       yalahoo::cameraFraming::PortraitPreviewSpan / aspect
                               + (portraitDetailSpan
                                  - yalahoo::cameraFraming::PortraitPreviewSpan / aspect)
                                       * detailProgress)
            : std::max(baseSpan,
                       baseSpan
                               * (yalahoo::cameraFraming::StandardAspectWidth
                                  / yalahoo::cameraFraming::AspectHeightUnits)
                               / aspect);
    const double yaw = (BrowseYawDegrees * radians + d->m_yaw.value) * (1 - detailProgress)
            + DetailYawDegrees * radians * detailProgress;
    const double elevation =
            (BrowseElevationDegrees * radians + d->m_elevation.value) * (1 - detailProgress)
            + DetailElevationDegrees * radians * detailProgress;
    const auto basis = cameraFraming::basis(yaw, elevation);
    const auto &direction = basis.direction, &right = basis.right, &up = basis.up;
    SceneVector aim{ -BrowseAimX, -BrowseAimY, BrowseAimZ };
    aim = cameraFraming::browseAim(aim, basis, span, aspect, d->m_layout.portrait());
    SceneVector detailAim{ 0,
                           cameraFraming::settledCaseBase()
                                   + (sceneGeometry::CaseHeight
                                      + yalahoo::sceneGeometry::InspectionLiftPadding)
                                   + sceneGeometry::HingeHeight,
                           -sceneGeometry::ArchiveDepth };
    const double detailX = d->m_layout.portrait() ? .5 : LandscapeDetailX;
    const double detailY = d->m_layout.portrait() ? PortraitDetailY + PortraitDetailTopPixels / h
            : d->m_layout.compact()
            ? CompactDetailY
            : DesktopDetailYPixels / yalahoo::cameraFraming::ReferenceHeightPixels;
    detailAim = detailAim + right * ((.5 - detailX) * w * span / h) + up * ((detailY - .5) * span);
    aim = aim * (1 - detailProgress) + detailAim * detailProgress;
    auto pose = OverviewController::framing(
            { aim
                      + direction
                              * (BrowseCameraDistance
                                 + (DetailCameraDistance - BrowseCameraDistance) * detailProgress),
              aim, span },
            w, h, d->m_selection ? d->m_selection->overviewProgress() : 0);
    auto position = pose.position;
    if (!d->m_reduced && detailProgress == 0) {
        position.x += d->m_pointerX * PointerParallaxDistance;
        position.y -= d->m_pointerY * PointerParallaxDistance;
    }
    return { position, pose.aim, pose.span };
}
void SceneAdapter::requestDetail(bool detail)
{
    if (!d->m_selection)
        return;
    d->m_managedDetail = true;
    d->m_selection->requestDetail(detail);
    if (d->m_reduced)
        advance(0);
}
void SceneAdapter::setDetail(double detail)
{
    if (!std::isfinite(detail))
        return;
    detail = std::clamp(detail, 0.0, 1.0);
    if (detail == d->m_detail)
        return;
    d->m_managedDetail = false;
    d->m_detail = detail;
    if (d->m_selection)
        d->m_selection->setDetail(detail);
    if (d->m_reduced)
        snapToTarget();
    else
        d->publish();
    Q_EMIT detailChanged();
}
void SceneAdapter::setReducedMotion(bool reduced)
{
    if (reduced == d->m_reduced)
        return;
    d->m_reduced = reduced;
    d->m_lighting.setReducedMotion(reduced);
    if (reduced) {
        d->m_yaw.finish(0);
        d->m_elevation.finish(0);
        if (d->m_managedDetail && d->m_selection)
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
    d->m_laneSpeed = laneSpeed;
    d->m_rowSpeed = realColumns ? 0 : rowSpeed;
}
void SceneAdapter::setPointer(double x, double y)
{
    if (!std::isfinite(x) || !std::isfinite(y))
        return;
    d->m_pointerX = std::clamp(x, -1.0, 1.0);
    d->m_pointerY = std::clamp(y, -1.0, 1.0);
}
void SceneAdapter::advance(double dt)
{
    if (d->m_openingTime || !std::isfinite(dt) || dt < 0)
        return;
    if (d->m_selection) {
        const auto speeds = d->m_selection->navigationSpeeds();
        setNavigation(speeds[0], speeds[1], d->m_selection->realistic());
    }
    if (d->m_managedDetail && d->m_selection) {
        const double detail = d->m_selection->detailMotion().value();
        if (d->m_detail != detail) {
            d->m_detail = detail;
            Q_EMIT detailChanged();
        }
    }
    if (d->m_reduced) {
        snapToTarget();
        if (d->m_managedDetail && d->m_selection)
            d->m_selection->updateDetailReadiness(dt, true);
        return;
    }
    d->m_yaw.update(std::clamp(d->m_laneSpeed / LaneOrbitSpeedScale, -1.0, 1.0) * LaneOrbitAmplitude
                            * (1 - d->m_detail),
                    OrbitFollowRate, dt);
    d->m_elevation.update(std::clamp(d->m_rowSpeed / RowOrbitSpeedScale, -1.0, 1.0)
                                  * RowOrbitAmplitude * (1 - d->m_detail),
                          OrbitFollowRate, dt);
    const auto goal = target();
    const auto p = components(goal.position), a = components(goal.aim);
    for (int i = 0; i < yalahoo::units::VectorAxes; ++i) {
        d->m_position[i].update(p[i], CameraFollowRate, dt);
        d->m_aim[i].update(a[i], CameraFollowRate, dt);
    }
    d->m_span.update(goal.span, CameraFollowRate, dt);
    d->publish();
    if (d->m_managedDetail && d->m_selection)
        d->m_selection->updateDetailReadiness(dt, settled());
}
void SceneAdapter::setOpeningTime(std::optional<double> time, double dt)
{
    if ((time && !std::isfinite(*time)) || !std::isfinite(dt))
        return;
    d->m_openingTime = time;
    d->m_managedDetail = false;
    d->m_detail = 0;
    d->m_yaw.finish(0);
    d->m_elevation.finish(0);
    if (time) {
        observe(openingCamera(*time, d->m_layout.width(), d->m_layout.height(),
                              d->m_selection ? d->m_selection->selectedPosition() : SceneVector{ }),
                dt);
    } else {
        observe(d->m_frame.camera, 0);
    }
    updateLighting(dt);
}
void SceneAdapter::observe(CameraPose pose, double dt)
{
    // Observe authored poses in the existing browse springs; the final hold clears velocity.
    const auto p = components(pose.position), a = components(pose.aim);
    const auto finite = [](double value) {
        return std::isfinite(value);
    };
    if (!std::ranges::all_of(p, finite) || !std::ranges::all_of(a, finite)
        || !std::isfinite(pose.span) || !std::isfinite(dt))
        return;
    for (int i = 0; i < yalahoo::units::VectorAxes; ++i) {
        d->m_position[i].velocity = dt > 0 ? (p[i] - d->m_position[i].value) / dt : 0;
        d->m_aim[i].velocity = dt > 0 ? (a[i] - d->m_aim[i].value) / dt : 0;
        d->m_position[i].value = p[i];
        d->m_aim[i].value = a[i];
    }
    d->m_span.velocity = dt > 0 ? (pose.span - d->m_span.value) / dt : 0;
    d->m_span.value = pose.span;
    d->publish();
}
void SceneAdapter::snapToTarget()
{
    const auto goal = target();
    const auto p = components(goal.position), a = components(goal.aim);
    for (int i = 0; i < yalahoo::units::VectorAxes; ++i) {
        d->m_position[i].finish(p[i]);
        d->m_aim[i].finish(a[i]);
    }
    d->m_span.finish(goal.span);
    d->publish();
}
bool SceneAdapter::settled() const
{
    const auto goal = target();
    return (values(d->m_position) - goal.position).length() < PositionSettledTolerance
            && (values(d->m_aim) - goal.aim).length() < AimSettledTolerance
            && std::abs(d->m_span.value - goal.span) < SpanSettledTolerance
            && velocities(d->m_position).length() < PositionVelocityTolerance
            && velocities(d->m_aim).length() < AimVelocityTolerance
            && std::abs(d->m_span.velocity) < SpanVelocityTolerance;
}
void SceneAdapter::Private::publish()
{
    const SceneVector position = values(m_position), aim = values(m_aim);
    const double renderedDistance = (position - aim).length();
    const double fogNear = renderedDistance + BrowseFogNearOffset * (1 - m_detail) - m_detail;
    const double fogFar =
            renderedDistance + BrowseFogFarOffset * (1 - m_detail) + DetailFogFarOffset * m_detail;
    const SceneVector caseOrigin = m_selection
            ? m_selection->selectedPosition()
            : SceneVector{ 0,
                           cameraFraming::settledCaseBase() + sceneGeometry::PreviewLift
                                   + (sceneGeometry::InspectionLift - sceneGeometry::PreviewLift)
                                           * m_detail,
                           -sceneGeometry::ArchiveDepth };
    const bool cameraChanged = !m_frame.revision || m_frame.width != m_layout.width()
            || m_frame.height != m_layout.height() || m_frame.camera.position != position
            || m_frame.camera.aim != aim || m_frame.camera.span != m_span.value;
    if (!cameraChanged && m_frame.hasArchivePool == bool(m_selection) && m_frame.fogNear == fogNear
        && m_frame.fogFar == fogFar && m_frame.casePosition == caseOrigin
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
    // Entity waves and cover changes do not alter camera matrices. Keep their exact values
    // until one of the camera/layout inputs changes instead of rebuilding/inverting per frame.
    if (cameraChanged) {
        const double distance = std::max(1.0, renderedDistance);
        m_frame.fieldOfView =
                2 * std::atan(std::max(MinimumFovSpan, m_span.value) / (2 * distance)) / radians;
        m_frame.view.setToIdentity();
        m_frame.view.lookAt(q->cameraPosition(), q->cameraAim(), { 0, 1, 0 });
        m_frame.cameraRotation =
                QQuaternion::fromRotationMatrix(m_frame.view.inverted().normalMatrix());
        m_frame.projection.setToIdentity();
        m_frame.projection.perspective(float(m_frame.fieldOfView),
                                       float(m_frame.width / m_frame.height), float(q->clipNear()),
                                       float(q->clipFar()));
        m_viewProjection = m_frame.projection * m_frame.view;
    }
    m_frame.casePosition = caseOrigin;
    m_frame.caseTransform.setToIdentity();
    m_frame.caseTransform.translate(q->casePosition());
    m_frame.hasArchivePool = bool(m_selection);
    m_frame.individualMaterials = m_managedDetail && bool(m_selection);
    m_frame.instances = m_selection ? m_selection->instances() : QVector<ArchiveInstance>{ };
    if (m_selection) {
        m_frame.caseTransform.rotate(m_selection->selectedRotation());
        const auto cell = m_selection->cell();
        q->setLightingInput({ caseOrigin, cell.row, cell.lane, m_selection->realistic() },
                            m_selection->selectedRotation());
    }
    Q_EMIT q->frameChanged();
}
void SceneAdapter::setLightingInput(RowLightingInput input, QQuaternion rotation)
{
    d->m_explicitLightingInput = true;
    d->m_lightingInput = input;
    d->m_caseRotation = rotation;
}
void SceneAdapter::updateLighting(double dt)
{
    auto input = d->m_lightingInput;
    if (!d->m_explicitLightingInput) {
        input.origin = d->m_frame.casePosition;
        input.row = input.origin.z / sceneGeometry::RowSpacing;
        input.lane = input.origin.x / sceneGeometry::ColumnSpacing;
    }
    input.reduced = d->m_reduced || input.reduced;
    input.cinematic = d->m_openingTime.has_value();
    input.introTime = d->m_openingTime;
    const auto localAim =
            d->m_caseRotation.rotatedVector({ -float(yalahoo::cameraFraming::LocalLightAimX),
                                              float(yalahoo::cameraFraming::LocalLightAimY), 0 });
    const auto offset = cameraRotation().rotatedVector({ -yalahoo::cameraFraming::LightOffsetX,
                                                         -yalahoo::cameraFraming::LightOffsetY,
                                                         yalahoo::cameraFraming::LightOffsetZ });
    d->m_lighting.update(input,
                         input.origin + SceneVector{ localAim.x(), localAim.y(), localAim.z() },
                         { offset.x(), offset.y(), offset.z() }, d->m_detail, dt);
}
double SceneAdapter::focalDepth(QVector3D origin) const
{
    // Frozen scene.ts: (model.position + (0,2,0)) in camera view space.
    return -(d->m_frame.view * QVector4D(origin + QVector3D(0, 2, 0), 1)).z();
}
std::optional<QVector3D> SceneAdapter::projectWorldPoint(QVector3D world) const
{
    return projectToViewport(d->m_viewProjection, world, { d->m_frame.width, d->m_frame.height });
}
QVariantMap SceneAdapter::hitTest(double x, double y) const
{
    if (d->m_selection)
        return d->m_selection->hitTest(d->m_frame, x, y);
    if (!std::isfinite(x) || !std::isfinite(y) || x < 0 || y < 0 || x >= d->m_frame.width
        || y >= d->m_frame.height)
        return { };
    bool invertible = false;
    const auto inverse = (d->m_frame.projection * d->m_frame.view * d->m_frame.caseTransform)
                                 .inverted(&invertible);
    if (!invertible)
        return { };
    const double nx = 2 * x / d->m_frame.width - 1, ny = 1 - 2 * y / d->m_frame.height;
    const auto start = (inverse * QVector4D(float(nx), float(ny), -1, 1)).toVector3DAffine();
    const auto end = (inverse * QVector4D(float(nx), float(ny), 1, 1)).toVector3DAffine();
    const auto delta = end - start;
    const auto enter = caseRayEntry(start, delta);
    if (!enter)
        return { };
    const auto local = start + delta * float(*enter);
    return { { u"objectId"_s, u"inspection-case"_s },
             { u"localPoint"_s, QVariant::fromValue(local) },
             { u"worldPoint"_s, QVariant::fromValue(d->m_frame.caseTransform.map(local)) },
             { u"frameRevision"_s, QVariant::fromValue(frameRevision()) } };
}
SelectionLighting *SceneAdapter::lighting()
{
    return &d->m_lighting;
}
RenderQuality *SceneAdapter::quality()
{
    return &d->m_quality;
}
QVariantMap SceneAdapter::renderBackendContract() const
{
    return RenderBackendContract::fields();
}
LayoutMetrics *SceneAdapter::layout()
{
    return &d->m_layout;
}
const SceneFrame &SceneAdapter::frame() const
{
    return d->m_frame;
}
QVector3D SceneAdapter::cameraPosition() const
{
    return d->m_frame.camera.position.qt();
}
QVector3D SceneAdapter::cameraAim() const
{
    return d->m_frame.camera.aim.qt();
}
QQuaternion SceneAdapter::cameraRotation() const
{
    return d->m_frame.cameraRotation;
}
QVector3D SceneAdapter::casePosition() const
{
    return d->m_frame.casePosition.qt();
}
double SceneAdapter::fieldOfView() const
{
    return d->m_frame.fieldOfView;
}
double SceneAdapter::clipNear() const
{
    return d->m_frame.nearPlane;
}
double SceneAdapter::clipFar() const
{
    return d->m_frame.farPlane;
}
quint64 SceneAdapter::frameRevision() const
{
    return d->m_frame.revision;
}
double SceneAdapter::detail() const
{
    return d->m_detail;
}
bool SceneAdapter::reducedMotion() const
{
    return d->m_reduced;
}
SceneAdapter::~SceneAdapter() = default;

}
