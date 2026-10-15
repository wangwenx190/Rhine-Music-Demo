#include "SceneAdapter.h"
#include "OverviewController.h"
#include "MotionFunctions.h"
#include <QVector4D>
#include <algorithm>
#include <numbers>
#include <limits>
#include <chrono>
namespace yalahoo {
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
            emit detailChanged();
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
            emit selectionControllerChanged();
        });
    }
    publish();
    emit selectionControllerChanged();
}
void SceneAdapter::recordInput()
{
    ++m_frame.inputRevision;
    m_frame.inputReceivedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                      std::chrono::steady_clock::now().time_since_epoch())
                                      .count();
    emit frameChanged();
}
void SceneAdapter::recordNavigationCommit(quint64 revision)
{
    m_frame.navigationRevision = revision;
    m_frame.navigationInputRevision = m_frame.inputRevision;
    m_frame.navigationCommittedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                            std::chrono::steady_clock::now().time_since_epoch())
                                            .count();
    emit frameChanged();
}
namespace {
constexpr double radians = std::numbers::pi / 180;
std::array<double, 3> components(SceneVector v)
{
    return { v.x, v.y, v.z };
}
SceneVector values(const std::array<ScalarSpring, 3> &s)
{
    return { s[0].value, s[1].value, s[2].value };
}
SceneVector velocities(const std::array<ScalarSpring, 3> &s)
{
    return { s[0].velocity, s[1].velocity, s[2].velocity };
}
}
SceneAdapter::SceneAdapter(FrameClock *clock, QObject *parent) : QObject(parent)
{
    // The first responsive frame starts at its target. Opening can observe its authored pose.
    snapToTarget();
    updateLighting(0);
    connect(&m_layout, &LayoutMetrics::changed, this, [this] {
        if (m_reduced)
            snapToTarget();
        else
            publish();
    });
    if (clock)
        connect(clock, &FrameClock::frame, this, [this](double, double motionDt) {
            advance(motionDt);
            updateLighting(motionDt);
        });
}
CameraPose SceneAdapter::target() const
{
    const double w = m_layout.width(), h = m_layout.height(), aspect = w / h, d = m_detail;
    const double baseSpan = 7.33 + (5.9 - 7.33) * d;
    const double portraitDetailSpan =
            std::max(6.3 / aspect, 3.7 * h / std::max(100.0, .54 * h - 156));
    const double span = m_layout.portrait()
            ? std::max(baseSpan, 8.4 / aspect + (portraitDetailSpan - 8.4 / aspect) * d)
            : std::max(baseSpan, baseSpan * (16.0 / 9) / aspect);
    const double yaw = (59 * radians + m_yaw.value) * (1 - d) + 8 * radians * d;
    const double elevation = (25 * radians + m_elevation.value) * (1 - d) + 20 * radians * d;
    const SceneVector direction{ -std::sin(yaw) * std::cos(elevation), std::sin(elevation),
                                 std::cos(yaw) * std::cos(elevation) };
    const auto right = cross({ 0, 1, 0 }, direction).normalized(),
               up = cross(direction, right).normalized();
    SceneVector aim{ -1.091, -.045, .481 };
    if (m_layout.portrait()) {
        aim = { 0, -4.6 + settlingWave(0, 26.56) + .4 + 1.85, -2.17 };
        aim = aim + up * ((.36 - .5) * span);
    } else {
        const double wide = smooth((aspect - 16.0 / 9) / (21.0 / 9 - 16.0 / 9));
        aim = aim + right * (.035 * wide * span * aspect) + up * ((.08 + .02 * wide) * span);
    }
    SceneVector detailAim{ 0, -4.6 + settlingWave(0, 26.56) + (3.35 + .12) + 1.85, -2.17 };
    const double detailX = m_layout.portrait() ? .5 : .25;
    const double detailY = m_layout.portrait() ? .27 + 34 / h
            : m_layout.compact()               ? .49
                                               : 560.0 / 1080;
    detailAim = detailAim + right * ((.5 - detailX) * w * span / h) + up * ((detailY - .5) * span);
    aim = aim * (1 - d) + detailAim * d;
    auto pose =
            OverviewController::framing({ aim + direction * (140 + (72 - 140) * d), aim, span }, w,
                                        h, m_selection ? m_selection->overviewProgress() : 0);
    auto position = pose.position;
    if (!m_reduced && d == 0) {
        position.x += m_pointerX * .12;
        position.y -= m_pointerY * .12;
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
    emit detailChanged();
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
    emit reducedMotionChanged();
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
    if (!std::isfinite(dt) || dt < 0)
        return;
    if (m_selection) {
        const auto speeds = m_selection->navigationSpeeds();
        setNavigation(speeds[0], speeds[1], m_selection->realistic());
    }
    if (m_managedDetail && m_selection) {
        const double detail = m_selection->detailMotion().value();
        if (m_detail != detail) {
            m_detail = detail;
            emit detailChanged();
        }
    }
    if (m_reduced) {
        snapToTarget();
        if (m_managedDetail && m_selection)
            m_selection->updateDetailReadiness(dt, true);
        return;
    }
    m_yaw.update(std::clamp(m_laneSpeed / 2.5, -1.0, 1.0) * .018 * (1 - m_detail), 6, dt);
    m_elevation.update(std::clamp(m_rowSpeed / 3, -1.0, 1.0) * .006 * (1 - m_detail), 6, dt);
    const auto goal = target();
    const auto p = components(goal.position), a = components(goal.aim);
    for (int i = 0; i < 3; ++i) {
        m_position[i].update(p[i], 9, dt);
        m_aim[i].update(a[i], 9, dt);
    }
    m_span.update(goal.span, 9, dt);
    publish();
    if (m_managedDetail && m_selection)
        m_selection->updateDetailReadiness(dt, settled());
}
void SceneAdapter::observe(CameraPose pose, double dt)
{
    // Preserve opening velocity for the later R20 handoff; no opening timeline here.
    const auto p = components(pose.position), a = components(pose.aim);
    for (int i = 0; i < 3; ++i) {
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
    for (int i = 0; i < 3; ++i) {
        m_position[i].finish(p[i]);
        m_aim[i].finish(a[i]);
    }
    m_span.finish(goal.span);
    publish();
}
bool SceneAdapter::settled() const
{
    const auto goal = target();
    return (values(m_position) - goal.position).length() < .035
            && (values(m_aim) - goal.aim).length() < .006
            && std::abs(m_span.value - goal.span) < .008 && velocities(m_position).length() < .09
            && velocities(m_aim).length() < .035 && std::abs(m_span.velocity) < .035;
}
void SceneAdapter::publish()
{
    const SceneVector position = values(m_position), aim = values(m_aim);
    const double renderedDistance = (position - aim).length();
    const double fogNear = renderedDistance + 5 * (1 - m_detail) - m_detail;
    const double fogFar = renderedDistance + 25 * (1 - m_detail) + 12 * m_detail;
    const SceneVector caseOrigin = m_selection
            ? m_selection->selectedPosition()
            : SceneVector{ 0, -4.6 + settlingWave(0, 26.56) + .9 + (3.47 - .9) * m_detail, -2.17 };
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
    m_frame.fieldOfView = 2 * std::atan(std::max(.1, m_span.value) / (2 * distance)) / radians;
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
    emit frameChanged();
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
        input.row = input.origin.z / .62;
        input.lane = input.origin.x / 5.2;
    }
    input.reduced = m_reduced || input.reduced;
    const auto localAim = m_caseRotation.rotatedVector({ -2.085f, 2.252f, 0 });
    const auto offset = cameraRotation().rotatedVector({ -48, -17.6f, 36 });
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
    const std::array<double, 3> lo{ -2.225, .175, -.07 }, hi{ 2.225, 3.525, .07 };
    for (int i = 0; i < 3; ++i) {
        if (std::abs(delta[i]) < 1e-10) {
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
    return { { "objectId", QStringLiteral("inspection-case") },
             { "localPoint", QVariant::fromValue(local) },
             { "worldPoint", QVariant::fromValue(m_frame.caseTransform.map(local)) },
             { "frameRevision", QVariant::fromValue(frameRevision()) } };
}
}
