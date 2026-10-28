#include "OverviewController.h"
#include "LayoutMetrics.h"
#include <utility>
#include "EasingCurve.h"
#include "SceneGeometry.h"
#include "FrameClock.h"
#include "MotionController.h"
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

struct OverviewController::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    explicit Private(OverviewController *owner, MotionController *motion)
        : q(owner), m_motion(motion), m_clock(motion ? motion->clock() : nullptr)
    {
    }
    OverviewController *const q;

    void notify();
    std::array<double, overviewMotion::NotificationFields> m_notified{ -1, -1, -1, -1, -1 };
    QPointer<MotionController> m_motion;
    QPointer<FrameClock> m_clock;
    QPointer<SceneAdapter> m_scene;
    QPointer<SelectionController> m_selection;
    QPointer<PresentationController> m_presentation;
    OverviewLabelsModel m_labels;
    SurfaceTransition m_chrome{ overviewMotion::ChromeEnterMilliseconds,
                                overviewMotion::ChromeExitMilliseconds, false, false };
    ScalarSpring m_progress;
    bool m_active = false, m_chromeShown = false, m_revealPending = false;
    quint64 m_sceneRevision = 0;
    bool m_labelsDirty = true;
    double m_navigationTime = overviewMotion::NavigationSlideSeconds, m_navFrom = 1,
           m_navOpacity = 1;
    double m_navOffsetFrom = 0, m_navOffset = 0;
};
OverviewController::OverviewController(MotionController *motion, QObject *parent)
    : QObject(parent), d(std::make_unique<Private>(this, motion))
{
    d->m_chrome.setEnterCurve(transformCurve());
    if (d->m_clock) {
        connect(d->m_clock, &FrameClock::frame, this, [this](double, double dt) {
            advance(dt, d->m_motion->reducedMotion());
        });
        connect(motion, &QObject::destroyed, this, &OverviewController::reset);
    }
}

void OverviewController::bind(SceneAdapter *scene, SelectionController *selection,
                              PresentationController *presentation)
{
    Q_ASSERT(!d->m_scene && !d->m_selection && !d->m_presentation);
    d->m_scene = scene;
    d->m_selection = selection;
    d->m_presentation = presentation;
    selection->setOverview(this);
    connect(scene, &QObject::destroyed, this, &OverviewController::reset);
    connect(selection, &QObject::destroyed, this, &OverviewController::reset);
    connect(presentation, &QObject::destroyed, this, &OverviewController::reset);
    connect(selection, &SelectionController::libraryChanged, this, &OverviewController::reset);
    connect(selection, &SelectionController::selectionChanged, this, &OverviewController::collapse);
    connect(selection, &SelectionController::arrayModeChanged, this, &OverviewController::collapse);
    connect(presentation, &PresentationController::changed, this, [this] {
        if (!d->m_presentation->archiveEnabled())
            setActive(false);
    });
    if (d->m_clock) {
        connect(d->m_clock, &FrameClock::frame, this, [this](double, double dt) {
            synchronize(dt, d->m_motion->reducedMotion());
        });
        connect(d->m_motion, &MotionController::reducedMotionChanged, this, [this] {
            advance(0, d->m_motion->reducedMotion());
            if (d->m_motion->reducedMotion()) {
                if (d->m_scene)
                    d->m_scene->setReducedMotion(true);
                if (d->m_selection)
                    d->m_selection->advance(0);
                if (d->m_scene)
                    d->m_scene->advance(0);
            }
            synchronize(0, d->m_motion->reducedMotion());
        });
    }
}
bool OverviewController::active() const
{
    return d->m_active;
}
bool OverviewController::ready() const
{
    return d->m_active && progress() > InteractionReadyProgress;
}
double OverviewController::progress() const
{
    return std::clamp(d->m_progress.value, 0.0, 1.0);
}
const Kinematics &OverviewController::state() const
{
    return d->m_progress;
}
OverviewLabelsModel *OverviewController::labels()
{
    return &d->m_labels;
}
SurfaceTransition *OverviewController::chrome()
{
    return &d->m_chrome;
}
double OverviewController::navigationOpacity() const
{
    return d->m_navOpacity;
}
double OverviewController::navigationOffset() const
{
    return d->m_navOffset;
}
void OverviewController::setActive(bool active)
{
    if (active == d->m_active)
        return;
    if (active
        && (!d->m_motion || !d->m_scene || !d->m_selection || d->m_selection->albumId().isEmpty()
            || !d->m_presentation || !d->m_presentation->archiveEnabled()))
        return;
    d->m_active = active;
    d->m_revealPending = !active;
    d->m_labels.collapse();
    d->m_labelsDirty = true;
    d->m_navFrom = d->m_navOpacity;
    d->m_navOffsetFrom = d->m_navOffset;
    d->m_navigationTime = 0;
    if (d->m_scene)
        d->m_scene->setPointer(0, 0);
    const bool reduced = d->m_clock && d->m_motion->reducedMotion();
    if (active) {
        d->m_presentation->browseSurface()->hide(reduced);
    } else {
        d->m_chromeShown = false;
        d->m_chrome.hide(reduced);
        Q_EMIT focusRequested(false);
    }
    if (reduced) {
        advance(0, true);
        if (d->m_selection)
            d->m_selection->advance(0);
        if (d->m_scene)
            d->m_scene->advance(0);
    }
    synchronize(0, reduced);
}
void OverviewController::toggle()
{
    setActive(!d->m_active);
}
void OverviewController::collapse()
{
    d->m_labels.collapse();
    d->m_labelsDirty = true;
}
void OverviewController::expand(int lane)
{
    if (ready() && d->m_labels.expand(lane)) {
        d->m_labelsDirty = true;
        synchronize(0, d->m_clock && d->m_motion->reducedMotion());
    }
}
bool OverviewController::enter(int lane)
{
    if (!ready() || !d->m_selection || !d->m_labels.canEnter(lane)
        || !d->m_selection->enterColumn(lane, true))
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
        d->m_progress.finish(d->m_active ? 1 : 0);
    else if (d->m_active || d->m_progress.value != 0 || d->m_progress.velocity != 0)
        d->m_progress.update(d->m_active ? 1 : 0, OverviewSpringRate, dt);
    d->m_navigationTime = reduced
            ? yalahoo::overviewMotion::NavigationSlideSeconds
            : std::min(yalahoo::overviewMotion::NavigationSlideSeconds, d->m_navigationTime + dt);

    d->m_navOpacity = d->m_navFrom
            + ((d->m_active ? 0 : 1) - d->m_navFrom)
                    * fadeCurve().valueForProgress(
                            std::min(1.0, d->m_navigationTime / NavigationFadeSeconds));
    d->m_navOffset = d->m_navOffsetFrom
            + ((d->m_active ? -NavigationHiddenOffsetPixels : 0) - d->m_navOffsetFrom)
                    * transformCurve().valueForProgress(
                            d->m_navigationTime / yalahoo::overviewMotion::NavigationSlideSeconds);
}
void OverviewController::synchronize(double dt, bool reduced)
{
    if (d->m_active && progress() > ChromeRevealProgress && !d->m_chromeShown) {
        d->m_chromeShown = true;
        d->m_chrome.show(reduced);
        Q_EMIT focusRequested(true);
    }
    d->m_chrome.advance(dt);
    if (reduced)
        d->m_chrome.finish();
    if (d->m_scene && (d->m_active || d->m_labels.liveCount())
        && (d->m_labelsDirty || d->m_sceneRevision != d->m_scene->frameRevision()
            || d->m_labels.running())) {
        d->m_sceneRevision = d->m_scene->frameRevision();
        d->m_labelsDirty = false;
        d->m_labels.update(projectColumns(), d->m_scene->layout()->width(),
                           d->m_scene->layout()->height(), ready(), dt, reduced);
    }
    if (d->m_revealPending && !d->m_active && d->m_scene && d->m_selection && d->m_presentation
        && d->m_presentation->archiveEnabled() && progress() < BrowseRevealProgress
        && d->m_selection->archiveReady() && d->m_scene->settled()) {
        d->m_revealPending = false;
        d->m_presentation->browseSurface()->show(reduced);
        Q_EMIT focusRequested(false);
    }
    d->notify();
}
void OverviewController::reset()
{
    d->m_active = d->m_chromeShown = d->m_revealPending = false;
    d->m_progress.finish(0);
    d->m_sceneRevision = 0;
    d->m_labelsDirty = true;
    d->m_chrome.reset(false);
    d->m_labels.clear();
    d->m_navigationTime = yalahoo::overviewMotion::NavigationSlideSeconds;
    d->m_navFrom = d->m_navOpacity = 1;
    d->m_navOffsetFrom = d->m_navOffset = 0;
    d->notify();
}
void OverviewController::Private::notify()
{
    const std::array<double, 5> state{ double(m_active), q->progress(), double(q->ready()),
                                       m_navOpacity, m_navOffset };
    if (state == m_notified)
        return;
    m_notified = state;
    Q_EMIT q->changed();
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
    if (!d->m_scene || !d->m_selection)
        return result;
    const auto &pool = d->m_selection->pool();
    const int count = pool.columnCount();
    if (!count)
        return result;
    result.reserve(std::min(count, int(2 * VisibleHalfLanes) + 1));
    const double center = d->m_selection->centerLane();
    // Enumerate physical candidates, then map to canonical IDs. Never scan C library columns.
    for (double physical = std::ceil(center - VisibleHalfLanes);
         physical <= center + VisibleHalfLanes; physical += 1) {
        if (d->m_selection->realistic() && (physical < 0 || physical >= count))
            continue;
        const int lane = ArchivePool::wrap(physical, count);
        const double nearest = d->m_selection->realistic()
                ? lane
                : std::floor((center - lane) / count + .5) * count + lane;
        // One nearest physical occurrence exists per canonical lane, so this test
        // also excludes duplicates without allocating an identity set each frame.
        if (physical != nearest || !pool.rowCount(lane))
            continue;
        const auto &column = pool.columns()[lane];
        const double extent = d->m_selection->columnExtent(physical);
        const double row = d->m_selection->browseRow(lane);
        const double anchorRow = d->m_selection->realistic()
                ? std::clamp(row, double(yalahoo::sceneGeometry::FirstAlbumRow),
                             double(ArchiveLastRowOffset + pool.rowCount(lane)))
                : row;
        const auto point = d->m_scene->projectWorldPoint(
                { float((physical - center) * sceneGeometry::ColumnSpacing),
                  float(-sceneGeometry::FloorDepth + sceneGeometry::CaseBottom
                        + (sceneGeometry::CaseHeight + LabelTopPadding) * extent),
                  float((anchorRow - row) * sceneGeometry::RowSpacing
                        - sceneGeometry::ArchiveDepth) });
        if (!point || point->z() < -1 || point->z() > 1)
            continue;
        result.append({ lane, column.label, pool.rowCount(lane), point->x(), point->y(), extent,
                        lane == ArchivePool::wrap(d->m_selection->cell().lane, count) });
    }
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
        return a.lane < b.lane;
    });
    return result;
}
OverviewController::~OverviewController() = default;

}
