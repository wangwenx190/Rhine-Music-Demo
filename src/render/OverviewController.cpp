#include "EasingCurve.h"
#include "SceneGeometry.h"
#include "FrameClock.h"
#include "MotionController.h"
#include "OverviewController.h"
#include "SceneAdapter.h"
#include "SelectionController.h"
#include "PresentationController.h"
#include <algorithm>
#include <numbers>

namespace yalahoo {
namespace {
// Overview framing and visibility thresholds use source scene units and normalized progress.
constexpr auto TransformBezierFirstX = .4;
constexpr auto TransformBezierSecondX = .2;
constexpr auto FadeBezierControlX = .25;
constexpr auto FadeBezierFirstY = .1;
constexpr auto InteractionReadyProgress = .75;
constexpr auto OverviewSpringRate = 6.5;
constexpr auto NavigationFadeSeconds = .28;
constexpr auto ChromeRevealProgress = .12;
constexpr auto BrowseRevealProgress = .02;
constexpr auto ColumnFoldProgress = .65;
constexpr auto LabelTopPadding = .65;
constexpr auto VisibleHalfLanes = 4.5;
constexpr auto OverviewYawDegrees = 28;
constexpr auto OverviewElevationDegrees = 43;
constexpr auto OverviewAimDepth = 1.8;
constexpr auto OverviewAimZ = 1.5;
constexpr auto OverviewCameraDistance = 172;
constexpr auto MinimumOverviewSpan = 16.8;
constexpr auto PortraitOverviewSpan = 17.5;
constexpr auto ArchiveLastRowOffset = 11;

// Qt's spline storage is initialized lazily and reused without per-frame allocation.
const QEasingCurve &transformCurve()
{
    static const auto value = cubicBezier(TransformBezierFirstX, 0, TransformBezierSecondX, 1);
    return value;
}
// Qt's spline storage is initialized lazily and reused without per-frame allocation.
const QEasingCurve &fadeCurve()
{
    static const auto value =
            cubicBezier(FadeBezierControlX, FadeBezierFirstY, FadeBezierControlX, 1);
    return value;
}
}

OverviewController::OverviewController(MotionController *motion, QObject *parent)
    : QObject(parent), m_motion(motion), m_clock(motion ? motion->clock() : nullptr)
{
    m_chrome.setEnterCurve(transformCurve());
    if (m_clock) {
        connect(m_clock, &FrameClock::frame, this, [this](double, double dt) {
            advance(dt, m_motion->reducedMotion());
        });
        connect(motion, &QObject::destroyed, this, &OverviewController::reset);
    }
}
void OverviewController::bind(SceneAdapter *scene, SelectionController *selection,
                              PresentationController *presentation)
{
    Q_ASSERT(!m_scene && !m_selection && !m_presentation);
    m_scene = scene;
    m_selection = selection;
    m_presentation = presentation;
    selection->setOverview(this);
    connect(scene, &QObject::destroyed, this, &OverviewController::reset);
    connect(selection, &QObject::destroyed, this, &OverviewController::reset);
    connect(presentation, &QObject::destroyed, this, &OverviewController::reset);
    connect(selection, &SelectionController::libraryChanged, this, &OverviewController::reset);
    connect(selection, &SelectionController::selectionChanged, this, &OverviewController::collapse);
    connect(selection, &SelectionController::arrayModeChanged, this, &OverviewController::collapse);
    connect(presentation, &PresentationController::changed, this, [this] {
        if (!m_presentation->archiveEnabled())
            setActive(false);
    });
    if (m_clock) {
        connect(m_clock, &FrameClock::frame, this, [this](double, double dt) {
            synchronize(dt, m_motion->reducedMotion());
        });
        connect(m_motion, &MotionController::reducedMotionChanged, this, [this] {
            advance(0, m_motion->reducedMotion());
            if (m_motion->reducedMotion()) {
                if (m_scene)
                    m_scene->setReducedMotion(true);
                if (m_selection)
                    m_selection->advance(0);
                if (m_scene)
                    m_scene->advance(0);
            }
            synchronize(0, m_motion->reducedMotion());
        });
    }
}
bool OverviewController::active() const
{
    return m_active;
}
bool OverviewController::ready() const
{
    return m_active && progress() > InteractionReadyProgress;
}
double OverviewController::progress() const
{
    return std::clamp(m_progress.value, 0.0, 1.0);
}
const Kinematics &OverviewController::state() const
{
    return m_progress;
}
OverviewLabelsModel *OverviewController::labels()
{
    return &m_labels;
}
SurfaceTransition *OverviewController::chrome()
{
    return &m_chrome;
}
double OverviewController::navigationOpacity() const
{
    return m_navOpacity;
}
double OverviewController::navigationOffset() const
{
    return m_navOffset;
}
void OverviewController::setActive(bool active)
{
    if (active == m_active)
        return;
    if (active
        && (!m_motion || !m_scene || !m_selection || m_selection->albumId().isEmpty()
            || !m_presentation || !m_presentation->archiveEnabled()))
        return;
    m_active = active;
    m_revealPending = !active;
    m_labels.collapse();
    m_labelsDirty = true;
    m_navFrom = m_navOpacity;
    m_navOffsetFrom = m_navOffset;
    m_navigationTime = 0;
    if (m_scene)
        m_scene->setPointer(0, 0);
    const bool reduced = m_clock && m_motion->reducedMotion();
    if (active) {
        m_presentation->browseSurface()->hide(reduced);
    } else {
        m_chromeShown = false;
        m_chrome.hide(reduced);
        Q_EMIT focusRequested(false);
    }
    if (reduced) {
        advance(0, true);
        if (m_selection)
            m_selection->advance(0);
        if (m_scene)
            m_scene->advance(0);
    }
    synchronize(0, reduced);
}
void OverviewController::toggle()
{
    setActive(!m_active);
}
void OverviewController::collapse()
{
    m_labels.collapse();
    m_labelsDirty = true;
}
void OverviewController::expand(int lane)
{
    if (ready() && m_labels.expand(lane)) {
        m_labelsDirty = true;
        synchronize(0, m_clock && m_motion->reducedMotion());
    }
}
bool OverviewController::enter(int lane)
{
    if (!ready() || !m_selection || !m_labels.canEnter(lane)
        || !m_selection->enterColumn(lane, true))
        return false;
    setActive(false);
    return true;
}
void OverviewController::albumClicked()
{
    setActive(false);
}
void OverviewController::advance(double dt, bool reduced)
{
    // Navigation opacity and position retarget from their displayed values on every reversal.
    constexpr auto NavigationHiddenOffsetPixels = 6;

    if (!std::isfinite(dt) || dt < 0)
        return;
    if (reduced)
        m_progress.finish(m_active ? 1 : 0);
    else if (m_active || m_progress.value != 0 || m_progress.velocity != 0)
        m_progress.update(m_active ? 1 : 0, OverviewSpringRate, dt);
    m_navigationTime = reduced
            ? yalahoo::overviewMotion::NavigationSlideSeconds
            : std::min(yalahoo::overviewMotion::NavigationSlideSeconds, m_navigationTime + dt);

    m_navOpacity = m_navFrom
            + ((m_active ? 0 : 1) - m_navFrom)
                    * fadeCurve().valueForProgress(
                            std::min(1.0, m_navigationTime / NavigationFadeSeconds));
    m_navOffset = m_navOffsetFrom
            + ((m_active ? -NavigationHiddenOffsetPixels : 0) - m_navOffsetFrom)
                    * transformCurve().valueForProgress(
                            m_navigationTime / yalahoo::overviewMotion::NavigationSlideSeconds);
}
void OverviewController::synchronize(double dt, bool reduced)
{
    if (m_active && progress() > ChromeRevealProgress && !m_chromeShown) {
        m_chromeShown = true;
        m_chrome.show(reduced);
        Q_EMIT focusRequested(true);
    }
    m_chrome.advance(dt);
    if (reduced)
        m_chrome.finish();
    if (m_scene && (m_active || m_labels.liveCount())
        && (m_labelsDirty || m_sceneRevision != m_scene->frameRevision() || m_labels.running())) {
        m_sceneRevision = m_scene->frameRevision();
        m_labelsDirty = false;
        m_labels.update(projectColumns(), m_scene->layout()->width(), m_scene->layout()->height(),
                        ready(), dt, reduced);
    }
    if (m_revealPending && !m_active && m_scene && m_selection && m_presentation
        && m_presentation->archiveEnabled() && progress() < BrowseRevealProgress
        && m_selection->archiveReady() && m_scene->settled()) {
        m_revealPending = false;
        m_presentation->browseSurface()->show(reduced);
        Q_EMIT focusRequested(false);
    }
    notify();
}
void OverviewController::reset()
{
    m_active = m_chromeShown = m_revealPending = false;
    m_progress.finish(0);
    m_sceneRevision = 0;
    m_labelsDirty = true;
    m_chrome.reset(false);
    m_labels.clear();
    m_navigationTime = yalahoo::overviewMotion::NavigationSlideSeconds;
    m_navFrom = m_navOpacity = 1;
    m_navOffsetFrom = m_navOffset = 0;
    notify();
}
void OverviewController::notify()
{
    const std::array<double, 5> state{ double(m_active), progress(), double(ready()), m_navOpacity,
                                       m_navOffset };
    if (state == m_notified)
        return;
    m_notified = state;
    Q_EMIT changed();
}
double OverviewController::columnExtent(double lane, double centerLane, double progress)
{
    const auto smoother = [](double value) {
        const double t = std::clamp(value, 0.0, 1.0);
        return t * t * t * (t * (t * 6 - 15) + 10);
    };
    return 1
            - smoother(progress / ColumnFoldProgress)
            * (1 - smoother(VisibleHalfLanes - std::abs(lane - centerLane)));
}
CameraPose OverviewController::framing(CameraPose base, double width, double height,
                                       double progress)
{
    const double p = std::clamp(progress, 0.0, 1.0);
    if (p == 0)
        return base;
    const double aspect = std::max(1.0, width) / std::max(1.0, height);
    constexpr double radians = std::numbers::pi / 180;
    const SceneVector wide{
        -std::sin(OverviewYawDegrees * radians) * std::cos(OverviewElevationDegrees * radians),
        std::sin(OverviewElevationDegrees * radians),
        std::cos(OverviewYawDegrees * radians) * std::cos(OverviewElevationDegrees * radians)
    };
    const double distance = (base.position - base.aim).length();
    const auto direction =
            ((base.position - base.aim).normalized() * (1 - p) + wide * p).normalized();
    const auto aim = base.aim * (1 - p) + SceneVector{ 0, -OverviewAimDepth, -OverviewAimZ } * p;
    return {
        aim + direction * (distance + (OverviewCameraDistance - distance) * p), aim,
        base.span + (std::max(MinimumOverviewSpan, PortraitOverviewSpan / aspect) - base.span) * p
    };
}
QVector<OverviewColumn> OverviewController::projectColumns() const
{
    QVector<OverviewColumn> result;
    if (!m_scene || !m_selection)
        return result;
    const auto &pool = m_selection->pool();
    const int count = pool.columnCount();
    if (!count)
        return result;
    result.reserve(std::min(count, int(2 * VisibleHalfLanes) + 1));
    const double center = m_selection->centerLane();
    // Enumerate physical candidates, then map to canonical IDs. Never scan C library columns.
    for (double physical = std::ceil(center - VisibleHalfLanes);
         physical <= center + VisibleHalfLanes; physical += 1) {
        if (m_selection->realistic() && (physical < 0 || physical >= count))
            continue;
        const int lane = ArchivePool::wrap(physical, count);
        const double nearest = m_selection->realistic()
                ? lane
                : std::floor((center - lane) / count + .5) * count + lane;
        // One nearest physical occurrence exists per canonical lane, so this test
        // also excludes duplicates without allocating an identity set each frame.
        if (physical != nearest || !pool.rowCount(lane))
            continue;
        const auto &column = pool.columns()[lane];
        const double extent = m_selection->columnExtent(physical);
        const double row = m_selection->browseRow(lane);
        const double anchorRow = m_selection->realistic()
                ? std::clamp(row, double(yalahoo::sceneGeometry::FirstAlbumRow),
                             double(ArchiveLastRowOffset + pool.rowCount(lane)))
                : row;
        const auto point = m_scene->projectWorldPoint(
                { float((physical - center) * sceneGeometry::ColumnSpacing),
                  float(-sceneGeometry::FloorDepth + sceneGeometry::CaseBottom
                        + (sceneGeometry::CaseHeight + LabelTopPadding) * extent),
                  float((anchorRow - row) * sceneGeometry::RowSpacing
                        - sceneGeometry::ArchiveDepth) });
        if (!point || point->z() < -1 || point->z() > 1)
            continue;
        result.append({ lane, column.label, pool.rowCount(lane), point->x(), point->y(), extent,
                        lane == ArchivePool::wrap(m_selection->cell().lane, count) });
    }
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
        return a.lane < b.lane;
    });
    return result;
}
}
