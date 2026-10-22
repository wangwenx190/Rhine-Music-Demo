#include "SceneMath.h"
#include "Units.h"
#include "OpeningTimeline.h"
#include "SceneGeometry.h"
#include "FrameClock.h"
#include "MotionController.h"
#include "SelectionController.h"
#include "OverviewController.h"
#include "MotionFunctions.h"
#include "MusicTypes.h"
#include <QVector4D>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
// Archive state uses stable IDs; bounded return copies, pulses and coordinate rebasing keep long sessions finite.
constexpr auto AdditionalCaseCapacity = 4;
constexpr auto IdleReleaseRate = 4;
constexpr auto MaximumPlaceholderTitleCharacters = 256;
constexpr auto RailOriginRow = 15.5;
constexpr auto InvisibleCaseThreshold = .0001;
constexpr auto AlreadyClearThreshold = .999;
constexpr auto LastAlbumRowOffset = 11;
constexpr auto MaximumCoordinateMagnitude = 1e12;
constexpr auto DetailPlacementThreshold = .99;
constexpr auto MaximumSelectionPulses = 6;
constexpr auto MinimumHitExtent = .12;
constexpr auto TrackPositionTolerance = .008;
constexpr auto TrackVelocityTolerance = .025;
constexpr auto AlignmentTolerance = .001;
constexpr auto ActivityThreshold = .01;
constexpr auto DetailOcclusionProgress = .9;
constexpr auto NeighborRadiusRows = 5;
constexpr auto NeighborClearance = .05;
constexpr auto MaximumRippleDisplacement = .24;
constexpr auto RippleLaneDistanceScale = 2.2;
constexpr auto OpeningEntryDepth = 23;
constexpr auto OpeningEntrySeconds = .75;
constexpr auto LiftPitchFadeDistance = .4;
constexpr auto SlopePitchGain = .024;
constexpr auto RoundoffUlps = 8;
constexpr auto PulseGainFollowRate = 8;
constexpr auto RebaseCoordinateMagnitude = 2048;
constexpr auto RebaseRowMagnitude = 2048.0;
constexpr auto ReducedFollowRate = 35.0;
constexpr auto ShoulderFollowRate = 5.0;
constexpr auto LaneFollowRate = 4.0;
constexpr auto ScanBlendDecayRate = 3;
constexpr auto TranslationColumn = 3;
constexpr auto PulseLifetimeSeconds = 3.2;
constexpr auto IdleDelaySeconds = 2.5;
constexpr auto IdleAttackRate = .8;
constexpr auto LiftFollowRate = 4.2;
constexpr auto ClarityReadyProgress = .78;
constexpr auto ClarityReadyLift = 3.3;
constexpr auto ReturnClarityDecayRate = 9;
constexpr auto DetailLiftRate = 9;
constexpr auto ReturnLiftRate = 4.5;
}

using namespace Qt::StringLiterals;

namespace yalahoo {
SelectionController::SelectionController(MotionController *motion, QObject *parent)
    : QObject(parent), m_motion(motion)
{
    m_instances.reserve(ArchivePool::capacity + AdditionalCaseCapacity);
    if (motion) {
        connect(motion->clock(), &FrameClock::frame, this, [this](double, double dt) {
            advance(dt);
        });
        connect(motion, &MotionController::reducedMotionChanged, this, [this] {
            advance(0);
        });
    }
}
void SelectionController::setColumns(QList<LibraryColumn> columns, const Snapshot &snapshot)
{
    const auto oldId = albumId();
    QString firstId;
    for (const auto &column : columns)
        if (!column.albumIds.isEmpty()) {
            firstId = column.albumIds.first();
            break;
        }
    m_pool.setColumns(std::move(columns));
    m_coverSources.clear();
    if (snapshot)
        m_coverSources.reserve(snapshot->albums.size());
    if (snapshot)
        for (const auto &album : snapshot->albums) {
            const auto entry = m_coverSources.tryEmplace(album.id);
            if (!entry.inserted)
                continue;
            const auto path = album.coverUrl.scheme() == "qrc"_L1 ? u":"_s + album.coverUrl.path()
                                                                  : album.coverUrl.toLocalFile();
            *entry.iterator = { { path,
                                  graphemePrefix(album.title, MaximumPlaceholderTitleCharacters) },
                                album.title };
        }
    m_returning.clear();
    m_drag.reset();
    m_detailMotion.reset();
    m_clarity.select();
    m_managedDetail = m_navigationLift = false;
    m_detail = 0;
    m_liftTarget = sceneGeometry::PreviewLift;
    m_poolObjects.fill({ });
    m_pulses.clear();
    m_origin = { 0, 0 };
    m_cell = m_pool.location(oldId).value_or(m_pool.location(firstId).value_or(
            ArchiveCell{ 0, yalahoo::sceneGeometry::FirstAlbumRow }));
    m_realColumns.reset(m_pool.rowCounts(), m_cell);
    m_memory.reset(m_pool.columns());
    m_lift.finish(m_liftTarget);
    m_rotation = 0;
    m_returnY.reset();
    m_selectedObject = ++m_objectSerial;
    if (m_motion) {
        m_motion->travel(-sceneGeometry::ArchiveDepth
                                 - (m_cell.row - RailOriginRow) * sceneGeometry::RowSpacing,
                         (m_cell.lane - 2) * sceneGeometry::ColumnSpacing, m_cell.row, m_cell.lane);
        m_motion->finish();
    }
    Q_EMIT libraryChanged();
    publish();
    Q_EMIT selectionChanged();
}
CoverSource SelectionController::coverSource(const QString &id) const
{
    const auto source = m_coverSources.constFind(id);
    return source == m_coverSources.cend() ? CoverSource{ } : source->cover;
}
QString SelectionController::albumTitle(const QString &id) const
{
    const auto source = m_coverSources.constFind(id);
    return source == m_coverSources.cend() ? QString{ } : source->title;
}
QString SelectionController::albumId() const
{
    return m_realistic ? m_pool.realAlbumAt(m_cell) : m_pool.albumAt(m_cell);
}
bool SelectionController::realistic() const
{
    return m_realistic;
}
const RealColumns &SelectionController::realColumns() const
{
    return m_realColumns;
}
double SelectionController::browseRow(int lane) const
{
    return m_realistic ? m_realColumns.row(lane, overviewProgress())
            : m_motion
            ? (-m_motion->rail() - sceneGeometry::ArchiveDepth) / sceneGeometry::RowSpacing
                    + RailOriginRow
            : m_cell.row;
}
void SelectionController::setOverview(OverviewController *overview)
{
    m_overview = overview;
}
double SelectionController::overviewProgress() const
{
    return m_overview ? m_overview->progress() : 0;
}
double SelectionController::centerLane() const
{
    return m_motion ? m_motion->column() / sceneGeometry::ColumnSpacing + 2 : m_cell.lane;
}
double SelectionController::columnExtent(double lane) const
{
    const double progress = overviewProgress();
    if (progress == 0)
        return 1;
    const double center = centerLane();
    if (center != m_extentCenter || progress != m_extentProgress) {
        m_extentCenter = center;
        m_extentProgress = progress;
        m_extentCache.fill({ });
    }
    auto &entry = m_extentCache[ArchivePool::wrap(lane, ArchivePool::lanes)];
    if (entry.lane != lane) {
        entry.lane = lane;
        entry.extent = OverviewController::columnExtent(lane, center, progress);
    }
    return entry.extent;
}
void SelectionController::overviewAlbumClicked()
{
    if (m_overview)
        m_overview->albumClicked();
}
bool SelectionController::rememberColumnPosition() const
{
    return m_memory.enabled;
}
void SelectionController::setRememberColumnPosition(bool enabled)
{
    if (enabled == m_memory.enabled)
        return;
    m_memory.enabled = enabled;
    if (enabled && m_pool.columnCount())
        m_memory.remember(
                m_pool.columns()[ArchivePool::wrap(m_cell.lane, m_pool.columnCount())].key,
                albumId());
    Q_EMIT memoryChanged();
}
NavigationAvailability SelectionController::navigationAvailability() const
{
    const auto location = m_pool.location(albumId());
    return location
            ? NavigationAvailability::at(m_pool.rowCount(location->lane),
                                         int(location->row - yalahoo::sceneGeometry::FirstAlbumRow),
                                         m_pool.columnCount(), m_realistic)
            : NavigationAvailability{ };
}
bool SelectionController::canMovePrevious() const
{
    return navigationAvailability().previous;
}
bool SelectionController::canMoveNext() const
{
    return navigationAvailability().next;
}
void SelectionController::setRealistic(bool realistic)
{
    if (m_realistic == realistic || !m_motion)
        return;
    const auto canonical = m_pool.location(albumId());
    const auto old = m_cell;
    const double displayed =
            (-m_motion->rail() - sceneGeometry::ArchiveDepth) / sceneGeometry::RowSpacing
            + RailOriginRow;
    m_realistic = realistic;
    m_cell = canonical.value_or(ArchiveCell{ 0, yalahoo::sceneGeometry::FirstAlbumRow });
    const ArchiveCell shift{ old.lane - m_cell.lane, old.row - m_cell.row };
    m_motion->translate({ shift.row * sceneGeometry::RowSpacing,
                          -shift.lane * sceneGeometry::ColumnSpacing, -shift.row, -shift.lane });
    m_realColumns.reset(m_pool.rowCounts(), m_cell, displayed - shift.row);
    m_origin = { 0, 0 };
    m_returning.clear();
    m_poolObjects.fill({ });
    m_pulses.clear();
    Q_EMIT arrayModeChanged();
    publish();
    Q_EMIT selectionChanged();
}
ArchiveCell SelectionController::cell() const
{
    return m_cell;
}
ArchiveCell SelectionController::coordinateOrigin() const
{
    return m_origin;
}
const ArchivePool &SelectionController::pool() const
{
    return m_pool;
}
const QVector<ArchiveInstance> &SelectionController::instances() const
{
    return m_instances;
}
quint64 SelectionController::revision() const
{
    return m_revision;
}
std::optional<ArchiveCell> SelectionController::selectionCell(const QString &id) const
{
    const auto canonical = m_pool.location(id);
    if (!canonical || m_realistic)
        return canonical;
    return ArchiveCell{
        ArchivePool::nearestOccurrence(canonical->lane, m_cell.lane, m_pool.columnCount()),
        ArchivePool::nearestOccurrence(canonical->row, m_cell.row, m_pool.rowCount(canonical->lane))
    };
}
bool SelectionController::albumNavigationEnabled(const QString &id, int direction) const
{
    const auto cell = m_pool.location(id.isEmpty() ? albumId() : id);
    return cell
            && NavigationAvailability::at(m_pool.rowCount(cell->lane),
                                          int(cell->row - yalahoo::sceneGeometry::FirstAlbumRow),
                                          m_pool.columnCount(), m_realistic)
                       .allowsRow(direction);
}
QString SelectionController::adjacentAlbum(const QString &id, int direction) const
{
    if (direction != 1 && direction != -1)
        return { };
    const auto cell = m_pool.location(id.isEmpty() ? albumId() : id);
    if (!cell || m_pool.rowCount(cell->lane) < 2)
        return { };
    const ArchiveCell target{ cell->lane, cell->row + direction };
    return m_realistic ? m_pool.realAlbumAt(target) : m_pool.albumAt(target);
}
bool SelectionController::canSelect(const QString &id) const
{
    const auto cell = selectionCell(id);
    return cell
            && (*cell == m_cell
                || m_returning.canExchange(*cell, m_lift.value > InvisibleCaseThreshold));
}
// Resolve the canonical cell and reserve return-copy capacity before changing identity.
// An interrupted selection retains the outgoing displayed pose instead of snapping it back.
bool SelectionController::select(const QString &id)
{
    const auto cell = selectionCell(id);
    return cell && selectCell(*cell, true);
}
bool SelectionController::switchDetail(const QString &id)
{
    if (!canSelect(id))
        return false;
    m_navigationLift = m_detailMotion.placed();
    const bool result = select(id);
    if (result) {
        m_drag.reset();
        m_clarity.enter(m_clarity.value() > AlreadyClearThreshold);
    }
    return result;
}
bool SelectionController::moveRow(int direction)
{
    if (!navigationAvailability().allowsRow(direction))
        return false;
    return selectCell({ m_cell.lane, m_cell.row + direction });
}
bool SelectionController::moveLane(int direction)
{
    if ((direction != 1 && direction != -1) || !navigationAvailability().left)
        return false;
    // Public selection accepts large periodic coordinates; narrow only after rebase.
    rebase();
    const double lane = m_cell.lane + direction;
    return enterColumn(int(lane), false);
}
bool SelectionController::enterColumn(int lane, bool guided)
{
    const auto target = columnTarget(lane, guided);
    return target && selectCell(*target, guided);
}
std::optional<ArchiveCell> SelectionController::columnTarget(int lane, bool guided) const
{
    if (!m_motion || !m_pool.columnCount())
        return std::nullopt;
    const int canonicalLane = ArchivePool::wrap(lane, m_pool.columnCount());
    const auto &column = m_pool.columns()[canonicalLane];
    if (column.albumIds.isEmpty())
        return std::nullopt;
    const auto remembered = m_pool.location(m_memory.recalled(column.key));
    double row;
    if (remembered && remembered->lane == canonicalLane)
        row = m_realistic ? remembered->row
                          : ArchivePool::nearestOccurrence(remembered->row, m_cell.row,
                                                           m_pool.rowCount(canonicalLane));
    else
        row = std::floor(browseRow(canonicalLane) + .5);
    if (m_realistic)
        row = std::clamp(row, double(yalahoo::sceneGeometry::FirstAlbumRow),
                         double(LastAlbumRowOffset + column.albumIds.size()));
    const double physicalLane = m_realistic ? double(canonicalLane)
            : guided
            ? ArchivePool::nearestOccurrence(canonicalLane,
                                             m_motion->column() / sceneGeometry::ColumnSpacing + 2,
                                             m_pool.columnCount())
            : double(lane);
    return ArchiveCell{ physicalLane, row };
}
bool SelectionController::navigateRow(double row)
{
    if (!std::isfinite(row))
        return false;
    // Slider/touch/detail callers share the same finite boundary as directional input.
    if (m_realistic
        && (row < yalahoo::sceneGeometry::FirstAlbumRow
            || row >= yalahoo::sceneGeometry::FirstAlbumRow + m_pool.rowCount(m_cell.lane)))
        return false;
    return selectCell({ m_cell.lane, row });
}
bool SelectionController::selectCell(ArchiveCell cell, bool guided)
{
    if (!m_motion || !std::isfinite(cell.lane) || !std::isfinite(cell.row)
        || std::abs(cell.lane) > MaximumCoordinateMagnitude
        || std::abs(cell.row) > MaximumCoordinateMagnitude || std::floor(cell.lane) != cell.lane
        || std::floor(cell.row) != cell.row
        || (m_realistic ? m_pool.realAlbumAt(cell) : m_pool.albumAt(cell)).isEmpty())
        return false;
    if (cell == m_cell)
        return true;
    if (!m_returning.canExchange(cell, m_lift.value > InvisibleCaseThreshold))
        return false;
    auto previous = m_returning.take(cell);
    if (m_lift.value > InvisibleCaseThreshold)
        m_returning.append(
                { m_cell, albumId(), m_lift, m_rotation,
                  m_rotation != 0 ? std::optional<double>(selectedPosition().y) : std::nullopt,
                  m_selectedObject, m_clarity.value() });
    const auto old = m_cell;
    if (m_managedDetail) {
        if (!m_detailMotion.placed())
            m_navigationLift = false;
        m_detailMotion.selectionChanged();
    }
    m_drag.reset();
    m_cell = cell;
    m_memory.remember(m_pool.columns()[ArchivePool::wrap(cell.lane, m_pool.columnCount())].key,
                      albumId());
    if (m_realistic)
        m_realColumns.select(cell, guided,
                             m_managedDetail ? m_detailMotion.placed()
                                             : m_detail > DetailPlacementThreshold);
    m_lastInteraction = m_time;
    m_lift = previous ? previous->lift : ScalarSpring{ };
    m_rotation = previous ? previous->rotation : 0;
    m_returnY = previous ? previous->returnY : std::nullopt;
    m_selectedObject = previous ? previous->objectId : ++m_objectSerial;
    m_clarity.select(previous ? previous->clarity : 0);
    const ArchiveValues targets{ -sceneGeometry::ArchiveDepth
                                         - (cell.row - RailOriginRow) * sceneGeometry::RowSpacing,
                                 (cell.lane - 2) * sceneGeometry::ColumnSpacing, cell.row,
                                 cell.lane };
    const bool distant = cell.lane != old.lane && std::abs(cell.row - old.row) > 1;
    const bool detailNavigation = m_managedDetail && m_navigationLift && m_detailMotion.placed();
    if (detailNavigation)
        m_motion->cancelTravel();
    else if (guided || distant || m_motion->moving())
        m_motion->retarget(targets);
    if (!detailNavigation)
        m_pulses.append({ cell, m_time, { } });
    if (m_pulses.size() > MaximumSelectionPulses)
        m_pulses.removeFirst();
    publish();
    Q_EMIT selectionChanged();
    return true;
}
void SelectionController::setLift(double lift)
{
    if (!std::isfinite(lift) || lift < 0 || lift > InspectionLift)
        return;
    m_liftTarget = lift;
    m_lift.finish(lift);
    publish();
}
void SelectionController::setRotation(double radians)
{
    if (!std::isfinite(radians))
        return;
    m_rotation = radians;
    publish();
}
void SelectionController::setDetail(double detail)
{
    if (!std::isfinite(detail))
        return;
    m_detail = std::clamp(detail, 0.0, 1.0);
    // Frozen music scene: MUSIC_MODEL.height + 0.12 clears the shelf.
    setLift(sceneGeometry::PreviewLift
            + (sceneGeometry::CaseHeight + yalahoo::sceneGeometry::InspectionLiftPadding
               - sceneGeometry::PreviewLift)
                    * m_detail);
}
void SelectionController::returnSelected()
{
    m_returnY = m_rotation != 0 ? std::optional<double>(selectedPosition().y) : std::nullopt;
    m_liftTarget = 0;
}
void SelectionController::requestDetail(bool detail)
{
    m_lastInteraction = m_time;
    m_managedDetail = true;
    m_detailMotion.request(detail);
    m_drag.cancel();
    if (detail) {
        m_clarity.enter();
        m_returnY.reset();
    } else {
        m_clarity.leave();
        if (m_rotation != 0)
            m_returnY = selectedPosition().y;
    }
    if (m_motion && m_motion->reducedMotion())
        advance(0);
}
std::array<double, 2> SelectionController::navigationSpeeds() const
{
    return m_motion ? std::array<double, 2>{ m_motion->springs()[ColumnAxis].velocity
                                                     / sceneGeometry::ColumnSpacing,
                                             m_motion->springs()[RailAxis].velocity
                                                     / sceneGeometry::RowSpacing }
                    : std::array<double, 2>{ 0, 0 };
}
bool SelectionController::tracksSettled() const
{
    if (!m_motion || m_motion->moving())
        return false;
    const ArchiveValues targets{ -sceneGeometry::ArchiveDepth
                                         - (m_cell.row - RailOriginRow) * sceneGeometry::RowSpacing,
                                 (m_cell.lane - 2) * sceneGeometry::ColumnSpacing, m_cell.row,
                                 m_cell.lane };
    for (size_t i = 0; i < targets.size(); ++i)
        if (std::abs(m_motion->springs()[i].value - targets[i]) >= TrackPositionTolerance
            || std::abs(m_motion->springs()[i].velocity) >= TrackVelocityTolerance)
            return false;
    return !m_realistic || m_realColumns.settled(int(m_cell.lane));
}
void SelectionController::updateDetailReadiness(double dt, bool cameraSettled)
{
    if (!m_managedDetail)
        return;
    const auto &d = m_detailMotion;
    const double target = d.holdsDetail() ? 1 : 0;
    const bool liftSettled = d.settled() && tracksSettled()
            && std::abs(m_detail - target) < AlignmentTolerance
            && std::abs(m_lift.value
                        - (target ? sceneGeometry::InspectionLift : sceneGeometry::PreviewLift))
                    < TrackPositionTolerance
            && std::abs(m_rotation) < AlignmentTolerance
            && std::abs(m_lift.velocity) < TrackVelocityTolerance;
    m_detailMotion.updateReadiness(dt, cameraSettled, liftSettled, m_motion->reducedMotion());
    if (m_detailMotion.phase() == DetailMotion::Phase::Archive)
        m_navigationLift = false;
}
bool SelectionController::archiveReady() const
{
    return tracksSettled()
            && (!m_managedDetail || m_detailMotion.phase() == DetailMotion::Phase::Archive);
}
bool SelectionController::archiveInteractive() const
{
    return archiveReady()
            || (m_detailMotion.phase() == DetailMotion::Phase::ReturningArray
                && m_detailMotion.settled() && m_detail < AlignmentTolerance
                && std::abs(m_rotation) < AlignmentTolerance);
}
bool SelectionController::canInspect() const
{
    if (!m_managedDetail || m_detailMotion.phase() != DetailMotion::Phase::Presented
        || m_detail <= DetailOcclusionProgress || m_pulseGain >= ActivityThreshold)
        return false;
    const auto top = [this](ArchiveCell cell, double lift = 0) {
        return -sceneGeometry::FloorDepth + field(cell.row, cell.lane) + lift
                + sceneGeometry::CaseTop;
    };
    double neighborTop = top({ m_cell.lane, m_cell.row - NeighborRadiusRows });
    for (int row = -NeighborRadiusRows + 1; row <= NeighborRadiusRows; ++row)
        if (row != 0)
            neighborTop = std::max(neighborTop, top({ m_cell.lane, m_cell.row + row }));
    for (const auto &copy : m_returning)
        if (copy.cell.lane == m_cell.lane
            && std::abs(copy.cell.row - m_cell.row) <= NeighborRadiusRows)
            neighborTop = std::max(neighborTop, top(copy.cell, copy.lift.value));
    return selectedPosition().y + sceneGeometry::CaseBottom - neighborTop > NeighborClearance;
}
bool SelectionController::beginDrag(QPointF point, int pointerId)
{
    if (!canInspect())
        return false;
    return m_drag.press(point, pointerId);
}
bool SelectionController::moveDrag(QPointF point, int pointerId)
{
    if (!canInspect()) {
        m_drag.cancel();
        return false;
    }
    return m_drag.move(point, pointerId);
}
void SelectionController::endDrag(QPointF point, int pointerId)
{
    m_drag.release(point, pointerId);
}
void SelectionController::cancelDrag()
{
    m_drag.cancel();
}
SelectionController::FieldColumn SelectionController::fieldColumn(double lane) const
{
    FieldColumn column;
    column.lane = lane;
    column.reduced = m_motion->reducedMotion();
    if (m_realistic) {
        column.activity = m_realColumns.activity(int(lane));
        column.center = m_realColumns.row(int(lane), m_openingTime ? 0 : overviewProgress());
    } else {
        const auto &springs = m_motion->springs();
        column.center = m_openingTime ? m_cell.row : springs[ShoulderAxis].value;
        column.strength =
                columnStrength(lane, m_openingTime ? m_cell.lane : springs[LaneAxis].value);
    }
    if (m_openingTime)
        column.intro = smooth((*m_openingTime - openingTimeline::PreviewStartSeconds)
                              / openingTimeline::PreviewDurationSeconds);
    return column;
}
double SelectionController::realisticField(double row, const FieldColumn &column) const
{
    const double lane = column.lane, activity = column.activity;
    double height =
            column.reduced ? 0 : idleWave(row, lane, m_time) * (1 + (m_idleGain - 1) * activity);
    if (!column.reduced && activity > 0 && m_pulseGain != 0) {
        double ripple = 0;
        for (const auto &pulse : m_pulses)
            if (pulse.cell.lane == lane)
                ripple += pulse.wave.sample(std::abs(row - pulse.cell.row));
        height += std::clamp(ripple, -MaximumRippleDisplacement, MaximumRippleDisplacement)
                * m_pulseGain * activity;
    }
    return height
            + settlingWave(row - column.center, sceneGeometry::SettledSceneSeconds) * activity;
}
double SelectionController::field(double row, double lane) const
{
    return m_motion ? field(row, fieldColumn(lane)) : 0;
}
double SelectionController::field(double row, const FieldColumn &column) const
{
    const double lane = column.lane;
    if (m_openingTime) {
        const double intro = column.intro;
        const double opening = intro == 1
                ? 0
                : cinematicField(row - m_cell.row + sceneGeometry::FirstAlbumRow,
                                 lane - m_cell.lane + 2, *m_openingTime);
        if (intro == 0)
            return opening;
        const double resting = settlingWave(row - column.center, sceneGeometry::SettledSceneSeconds)
                * (m_realistic ? column.activity : column.strength);
        return opening * (1 - intro) + resting * intro;
    }
    if (m_realistic)
        return realisticField(row, column);
    double height = 0;
    if (m_scanBlend != 0)
        height += archiveWave(row + m_origin.row, lane + m_origin.lane, m_scanTime) * m_scanBlend;
    if (m_idleGain != 0)
        height += idleWave(row + m_origin.row, lane + m_origin.lane, m_time) * m_idleGain;
    if (!column.reduced && m_pulseGain != 0) {
        double ripple = 0;
        for (const auto &pulse : m_pulses)
            ripple += pulse.wave.sample(std::hypot(
                    row - pulse.cell.row, (lane - pulse.cell.lane) * RippleLaneDistanceScale));
        height += std::clamp(ripple, -MaximumRippleDisplacement, MaximumRippleDisplacement)
                * m_pulseGain;
    }
    return height
            + settlingWave(row - column.center, sceneGeometry::SettledSceneSeconds)
            * column.strength;
}
QMatrix4x4 SelectionController::transform(ArchiveCell cell, double lift, double rotation,
                                          bool folded) const
{
    QMatrix4x4 matrix;
    if (!m_motion)
        return matrix;
    return transform(cell, lift, rotation, folded, transformInputs(), field(cell.row, cell.lane),
                     field(cell.row + .5, cell.lane) - field(cell.row - .5, cell.lane));
}
SelectionController::TransformInputs SelectionController::transformInputs() const
{
    const double entryZ = m_openingTime ? -OpeningEntryDepth
                    * std::pow(1
                                       - std::clamp((*m_openingTime
                                                     - yalahoo::openingTimeline::SceneOriginSeconds)
                                                            / OpeningEntrySeconds,
                                                    0.0, 1.0),
                               2)
                                        : 0;
    return { displayOffset(), entryZ, overviewProgress() };
}
QMatrix4x4 SelectionController::transform(ArchiveCell cell, double lift, double rotation,
                                          bool folded, const TransformInputs &inputs, double height,
                                          double slope) const
{
    QMatrix4x4 matrix;
    const auto &offset = inputs.offset;
    const double entryZ = inputs.entryZ;
    matrix.translate(float((cell.lane - m_cell.lane) * sceneGeometry::ColumnSpacing - offset[1]),
                     float(-sceneGeometry::FloorDepth + height + lift),
                     float(m_realistic
                                   ? (cell.row - m_realColumns.row(int(cell.lane), inputs.overview))
                                                   * sceneGeometry::RowSpacing
                                           - sceneGeometry::ArchiveDepth + entryZ
                                   : (cell.row - m_cell.row) * sceneGeometry::RowSpacing
                                           - sceneGeometry::ArchiveDepth + offset[0] + entryZ));
    const double quality = smooth(lift / LiftPitchFadeDistance);
    const double pitch = slope * SlopePitchGain * (1 - m_detail) * (1 - quality);
    // Evaluate Three's XYZ rotation in double, then round each matrix element once.
    // A float quaternion product adds avoidable normal/depth error for optical shells.
    const double cx = std::cos(pitch), sx = std::sin(pitch);
    const double cy = std::cos(rotation), sy = std::sin(rotation);
    QMatrix4x4 orientation;
    orientation(0, 0) = float(cy);
    orientation(0, 1) = 0;
    orientation(0, 2) = float(sy);
    orientation(1, 0) = float(sx * sy);
    orientation(1, 1) = float(cx);
    orientation(1, 2) = float(-sx * cy);
    orientation(2, 0) = float(-cx * sy);
    orientation(2, 1) = float(sx);
    orientation(2, 2) = float(cx * cy);
    matrix *= orientation;
    const double extent = folded ? columnExtent(cell.lane) : 1;
    if (extent <= 0) {
        matrix.scale(0);
    } else if (extent != 1) {
        matrix.translate(0, float(sceneGeometry::CaseBottom * (1 - extent)), 0);
        matrix.scale(1, float(extent), 1);
    }
    return matrix;
}
std::array<double, 2> SelectionController::displayOffset() const
{
    const auto &s = m_motion->springs();
    const std::array<double, 2> target{
        -sceneGeometry::ArchiveDepth - (m_cell.row - RailOriginRow) * sceneGeometry::RowSpacing,
        (m_cell.lane - 2) * sceneGeometry::ColumnSpacing
    };
    std::array<double, 2> offset;
    for (int i = 0; i < 2; ++i) {
        offset[i] = s[i].value - target[i];
        // Translate can accumulate a few ulps. Preserve exact integer recycle ties at rest.
        const double roundoff = RoundoffUlps * std::numeric_limits<double>::epsilon()
                * std::max({ 1.0, std::abs(s[i].value), std::abs(target[i]),
                             std::abs(target[i])
                                     + (i == 0 ? std::abs(m_origin.row * sceneGeometry::RowSpacing)
                                               : std::abs(m_origin.lane
                                                          * sceneGeometry::ColumnSpacing)) });
        if (!m_motion->moving() && std::abs(offset[i]) <= roundoff)
            offset[i] = 0;
    }
    return offset;
}
SceneVector SelectionController::selectedPosition() const
{
    const auto p = transform(m_cell, m_lift.value, m_rotation).column(TranslationColumn);
    return { p.x(), p.y(), p.z() };
}
QQuaternion SelectionController::selectedRotation() const
{
    return QQuaternion::fromRotationMatrix(
            transform(m_cell, m_lift.value, m_rotation, false).normalMatrix());
}
// Translate displayed tracks and remembered origins by the same whole-period offset.
// This bounds floating-point coordinates without changing the visible pose or canonical album ID.
ArchiveCell SelectionController::rebase()
{
    ArchiveCell shift{ 0, 0 };
    if (!m_motion || m_realistic)
        return shift;
    const auto period = m_pool.rowPeriod();
    const int lanes = qMax(1, m_pool.columnCount());
    if (std::abs(m_cell.lane) > RebaseCoordinateMagnitude)
        shift.lane = std::floor((m_cell.lane - 2) / lanes + .5) * lanes;
    if (period && std::abs(m_cell.row) > std::max(RebaseRowMagnitude, double(*period) * 2))
        shift.row = std::floor((m_cell.row - yalahoo::sceneGeometry::FirstAlbumRow) / *period)
                * *period;
    if (shift == ArchiveCell{ 0, 0 })
        return shift;
    m_cell.lane -= shift.lane;
    m_cell.row -= shift.row;
    m_origin.lane += shift.lane;
    m_origin.row += shift.row;
    for (auto &copy : m_returning) {
        copy.cell.lane -= shift.lane;
        copy.cell.row -= shift.row;
    }
    for (auto &pulse : m_pulses) {
        pulse.cell.lane -= shift.lane;
        pulse.cell.row -= shift.row;
    }
    m_motion->translate({ shift.row * sceneGeometry::RowSpacing,
                          -shift.lane * sceneGeometry::ColumnSpacing, -shift.row, -shift.lane });
    Q_EMIT coordinatesRebased(shift.lane, shift.row);
    publish();
    return shift;
}
void SelectionController::setOpeningTime(std::optional<double> time)
{
    if (time && !std::isfinite(*time))
        return;
    const bool entering = time && !m_openingTime;
    m_openingTime = time;
    m_scanBlend = m_idleGain = 0;
    m_pulses.clear();
    m_lastInteraction = m_time;
    if (time) {
        m_returning.clear();
        m_rotation = m_detail = 0;
        m_returnY.reset();
        m_managedDetail = m_navigationLift = false;
        m_detailMotion.reset();
        m_clarity.select();
        const double intro = smooth((*time - yalahoo::openingTimeline::PreviewStartSeconds)
                                    / yalahoo::openingTimeline::PreviewDurationSeconds);
        m_lift.finish(extraction(*time) * (1 - intro) + sceneGeometry::PreviewLift * intro);
        m_liftTarget = sceneGeometry::PreviewLift;
        if (entering && m_motion) {
            m_motion->travel(-sceneGeometry::ArchiveDepth
                                     - (m_cell.row - RailOriginRow) * sceneGeometry::RowSpacing,
                             (m_cell.lane - 2) * sceneGeometry::ColumnSpacing, m_cell.row,
                             m_cell.lane);
            m_motion->finish();
        }
        m_realColumns.update(0, true);
    }
    publish();
}
void SelectionController::advanceLift(double dt, bool reduced, bool detailNavigation)
{
    m_returnY.reset();
    const bool aligningNeighbor =
            std::any_of(m_returning.cbegin(), m_returning.cend(), [&](const Copy &copy) {
                return copy.returnY && copy.cell.lane == m_cell.lane
                        && std::abs(copy.cell.row - m_cell.row) < NeighborRadiusRows;
            });
    if (m_managedDetail && m_navigationLift) {
        m_liftTarget = m_detailMotion.placed()
                ? (!reduced && aligningNeighbor ? 0 : sceneGeometry::InspectionLift)
                : sceneGeometry::PreviewLift;
        if (reduced)
            m_lift.finish(m_liftTarget);
        else
            m_lift.update(m_liftTarget, detailNavigation ? DetailLiftRate : LiftFollowRate, dt);
    } else if (m_managedDetail
               && (m_detail > 0 || !m_detailMotion.settled() || m_detailMotion.holdsDetail())) {
        m_lift.value = sceneGeometry::PreviewLift
                + (sceneGeometry::InspectionLift - sceneGeometry::PreviewLift) * m_detail;
        m_lift.velocity = m_detailMotion.state().value == m_detail
                ? (sceneGeometry::InspectionLift - sceneGeometry::PreviewLift)
                        * m_detailMotion.state().velocity
                : 0;
    } else if (reduced)
        m_lift.finish(m_liftTarget);
    else
        m_lift.update(m_liftTarget, LiftFollowRate, dt);
}
void SelectionController::advance(double dt)
{
    if (m_openingTime)
        return;
    if (!m_motion || m_pool.columnCount() == 0 || !std::isfinite(dt) || dt < 0)
        return;
    rebase();
    const bool reduced = m_motion->reducedMotion();
    const ArchiveValues targets{ -sceneGeometry::ArchiveDepth
                                         - (m_cell.row - RailOriginRow) * sceneGeometry::RowSpacing,
                                 (m_cell.lane - 2) * sceneGeometry::ColumnSpacing, m_cell.row,
                                 m_cell.lane };
    const bool detailNavigation = m_managedDetail && m_navigationLift && m_detailMotion.placed();
    const ArchiveValues rates{ reduced ? ReducedFollowRate
                                       : detailNavigation
                                       ? yalahoo::sceneGeometry::DetailFollowRate
                                       : yalahoo::sceneGeometry::BrowseFollowRate,
                               reduced ? ReducedFollowRate
                                       : detailNavigation
                                       ? yalahoo::sceneGeometry::DetailFollowRate
                                       : yalahoo::sceneGeometry::BrowseFollowRate,
                               reduced ? ReducedFollowRate
                                       : detailNavigation ? yalahoo::sceneGeometry::DetailFollowRate
                                                          : ShoulderFollowRate,
                               reduced ? ReducedFollowRate
                                       : detailNavigation ? yalahoo::sceneGeometry::DetailFollowRate
                                                          : LaneFollowRate };
    if (!m_motion->moving()) {
        auto springs = m_motion->springs();
        for (int i = 0; i < ArchiveAxisCount; ++i) {
            if (reduced)
                springs[i].finish(targets[i]);
            else
                springs[i].update(targets[i], rates[i], dt);
        }
        m_motion->observe(springs, targets, rates);
    }
    m_time += dt;
    if (m_realistic)
        m_realColumns.update(dt, reduced);
    m_scanTime += dt;
    m_scanBlend *= std::exp(-dt * ScanBlendDecayRate);
    m_pulses.removeIf([&](const Pulse &p) {
        return m_time - p.time >= PulseLifetimeSeconds;
    });
    for (auto &pulse : m_pulses)
        pulse.wave = prepareMusicSelectionWave(m_time - pulse.time);
    const bool aligning =
            std::any_of(m_returning.cbegin(), m_returning.cend(), [](const Copy &copy) {
                return copy.returnY.has_value();
            });
    const bool idle = !reduced && m_liftTarget > 0 && m_detail < ActivityThreshold && !m_returnY
            && !aligning && m_time - m_lastInteraction > IdleDelaySeconds;
    m_idleGain += ((idle ? 1 : 0) - m_idleGain)
            * (1 - std::exp(-dt * (idle ? IdleAttackRate : IdleReleaseRate)));
    const bool mutePulse = m_detail > 0 || m_returnY || aligning;
    m_pulseGain += ((mutePulse ? 0 : 1) - m_pulseGain) * (1 - std::exp(-dt * PulseGainFollowRate));
    if (reduced)
        m_idleGain = 0;
    const bool inspecting = m_managedDetail && m_detailMotion.placed() && !m_returnY;
    m_rotation = m_drag.advance(m_rotation, dt, inspecting, reduced);
    if (m_managedDetail) {
        m_detailMotion.advance(dt, reduced, m_rotation == 0);
        m_detail = m_detailMotion.value();
    }
    const double base = -sceneGeometry::FloorDepth + field(m_cell.row, m_cell.lane);
    if (m_returnY && m_rotation != 0)
        m_lift.finish(*m_returnY - base);
    else {
        advanceLift(dt, reduced, detailNavigation);
    }
    if (m_managedDetail)
        m_clarity.advance(dt, m_detail > ClarityReadyProgress && m_lift.value > ClarityReadyLift,
                          reduced);
    for (auto &copy : m_returning) {
        copy.clarity = reduced ? 0 : copy.clarity * std::exp(-dt * ReturnClarityDecayRate);
        copy.rotation = reduced ? 0 : returnStep(copy.rotation, dt, false);
        if (copy.returnY && !reduced) {
            copy.lift.finish(*copy.returnY + sceneGeometry::FloorDepth
                             - field(copy.cell.row, copy.cell.lane));
            if (copy.rotation == 0)
                copy.returnY.reset();
        } else {
            copy.returnY.reset();
            if (reduced)
                copy.lift.finish(0);
            else
                copy.lift.update(0, detailNavigation ? DetailLiftRate : ReturnLiftRate, dt);
        }
    }
    m_returning.removeIf([](const Copy &copy) {
        return copy.lift.value < InvisibleCaseThreshold
                && std::abs(copy.rotation) < InvisibleCaseThreshold;
    });
    publish();
}
void SelectionController::publish()
{
    if (!m_motion)
        return;
    const auto offset = displayOffset();
    const ArchiveCell center{ m_cell.lane + offset[1] / sceneGeometry::ColumnSpacing,
                              m_cell.row - offset[0] / sceneGeometry::RowSpacing };
    std::array<double, ArchivePool::lanes> rows;
    if (m_realistic)
        for (int index = 0; index < ArchivePool::lanes; ++index)
            rows[index] = m_realColumns.row(
                    int(ArchivePool::visibleCell(index * ArchivePool::rows, center).lane),
                    overviewProgress());
    m_pool.update(center, m_realistic ? &rows : nullptr);
    auto &next = m_instanceScratch;
    next.clear();
    next.reserve(ArchivePool::capacity + AdditionalCaseCapacity);
    const auto inputs = transformInputs();
    struct PreviousField final
    {
        ArchiveCell cell;
        double upper;
    };
    std::optional<PreviousField> previousField;
    std::optional<FieldColumn> column;
    for (auto instance : m_pool.instances()) {
        // Optical depth ties use the physical identity, which survives logical rebase.
        const PoolObject::PhysicalCell physical{ qint64(instance.cell.lane + m_origin.lane),
                                                 qint64(instance.cell.row + m_origin.row) };
        const auto lane =
                (physical.lane % ArchivePool::lanes + ArchivePool::lanes) % ArchivePool::lanes;
        const auto row = (physical.row % ArchivePool::rows + ArchivePool::rows) % ArchivePool::rows;
        auto &object = m_poolObjects[lane * ArchivePool::rows + row];
        if (!object.objectId || object.physical != physical)
            object = { physical, ++m_objectSerial };
        instance.objectId = object.objectId;
        const bool hidden = instance.cell == m_cell
                || std::any_of(m_returning.cbegin(), m_returning.cend(), [&](const Copy &copy) {
                                return copy.cell == instance.cell;
                            });
        if (hidden || instance.albumId.isEmpty())
            continue;
        // Consecutive rows share the same half-row sample in the slope difference.
        // Reuse the exact value only within this publication, so every state change
        // naturally invalidates it without a persistent field-cache key.
        const auto cell = instance.cell;
        if (!column || column->lane != cell.lane)
            column = fieldColumn(cell.lane);
        const double lower = previousField && previousField->cell.lane == cell.lane
                        && previousField->cell.row + 1 == cell.row
                ? previousField->upper
                : field(cell.row - .5, *column);
        const double upper = field(cell.row + .5, *column);
        previousField = PreviousField{ cell, upper };
        instance.transform =
                transform(cell, 0, 0, true, inputs, field(cell.row, *column), upper - lower);
        next.append(std::move(instance));
    }
    for (const auto &copy : m_returning)
        next.append({ copy.cell, copy.albumId, copy.objectId,
                      transform(copy.cell, copy.lift.value, copy.rotation), true, copy.clarity });
    if (!albumId().isEmpty())
        next.append({ m_cell, albumId(), m_selectedObject,
                      transform(m_cell, m_lift.value, m_rotation), true, m_clarity.value() });
    if (next == m_instances)
        return;
    m_instances.swap(next);
    ++m_revision;
    Q_EMIT frameChanged();
}
QVariantMap SelectionController::hitTest(const SceneFrame &frame, double x, double y) const
{
    if (!std::isfinite(x) || !std::isfinite(y) || x < 0 || y < 0 || x >= frame.width
        || y >= frame.height)
        return { };
    bool valid = false;
    const auto inverse = (frame.projection * frame.view).inverted(&valid);
    if (!valid)
        return { };
    const float nx = float(2 * x / frame.width - 1), ny = float(1 - 2 * y / frame.height);
    const auto start = (inverse * QVector4D(nx, ny, -1, 1)).toVector3DAffine();
    const auto end = (inverse * QVector4D(nx, ny, 1, 1)).toVector3DAffine();
    double nearest = 2;
    QVariantMap hit;
    for (const auto &instance : frame.instances) {
        if (columnExtent(instance.cell.lane) <= MinimumHitExtent)
            continue;
        const auto localInverse = instance.transform.inverted(&valid);
        if (!valid)
            continue;
        const auto localStart = localInverse.map(start), delta = localInverse.map(end) - localStart;
        const auto enter = caseRayEntry(localStart, delta);
        if (!enter || *enter >= nearest)
            continue;
        nearest = *enter;
        const auto local = localStart + delta * float(*enter);
        hit = { { u"albumId"_s, instance.albumId },
                { u"objectId"_s, QVariant::fromValue(instance.objectId) },
                { u"lane"_s, instance.cell.lane },
                { u"row"_s, instance.cell.row },
                { u"localPoint"_s, QVariant::fromValue(local) },
                { u"worldPoint"_s, QVariant::fromValue(instance.transform.map(local)) },
                { u"frameRevision"_s, QVariant::fromValue(frame.revision) } };
    }
    return hit;
}
}
