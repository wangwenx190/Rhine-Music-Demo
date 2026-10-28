#include "SelectionController.h"
#include <QPointer>
#include "PointerDragController.h"
#include "CaseClarity.h"
#include "ArchiveField.h"
#include <utility>
#include "SceneMath.h"
#include "Units.h"
#include "OpeningTimeline.h"
#include "SceneGeometry.h"
#include "FrameClock.h"
#include "MotionController.h"
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
constexpr auto MaximumPlaceholderTitleCharacters = 256;
constexpr auto RailOriginRow = 15.5;
constexpr auto InvisibleCaseThreshold = .0001;
constexpr auto AlreadyClearThreshold = .999;
constexpr auto LastAlbumRowOffset = 11;
constexpr auto MaximumCoordinateMagnitude = 1e12;
constexpr auto DetailPlacementThreshold = .99;
constexpr auto MinimumHitExtent = .12;
constexpr auto TrackPositionTolerance = .008;
constexpr auto TrackVelocityTolerance = .025;
constexpr auto AlignmentTolerance = .001;
constexpr auto ActivityThreshold = .01;
constexpr auto DetailOcclusionProgress = .9;
constexpr auto NeighborRadiusRows = 5;
constexpr auto NeighborClearance = .05;
constexpr auto OpeningEntryDepth = 23;
constexpr auto OpeningEntrySeconds = .75;
constexpr auto LiftPitchFadeDistance = .4;
constexpr auto SlopePitchGain = .024;
constexpr auto RoundoffUlps = 8;
constexpr auto RebaseCoordinateMagnitude = 2048;
constexpr auto RebaseRowMagnitude = 2048.0;
constexpr auto ReducedFollowRate = 35.0;
constexpr auto ShoulderFollowRate = 5.0;
constexpr auto LaneFollowRate = 4.0;
constexpr auto TranslationColumn = 3;
constexpr auto LiftFollowRate = 4.2;
constexpr auto ClarityReadyProgress = .78;
constexpr auto ClarityReadyLift = 3.3;
constexpr auto ReturnClarityDecayRate = 9;
constexpr auto DetailLiftRate = 9;
constexpr auto ReturnLiftRate = 4.5;
}

using namespace Qt::StringLiterals;

namespace yalahoo {
struct SelectionController::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    explicit Private(SelectionController *owner, MotionController *motion)
        : q(owner), m_motion(motion)
    {
    }
    SelectionController *const q;

    ArchiveField::Column fieldColumn(double lane) const;
    double field(double row, const ArchiveField::Column &column) const;
    void advanceLift(double dt, bool reduced, bool detailNavigation);
    void publish();
    bool tracksSettled() const;
    std::optional<ArchiveCell> selectionCell(const QString &id) const;
    double field(double row, double lane) const;
    std::array<double, 2> displayOffset() const;
    QMatrix4x4 transform(ArchiveCell cell, double lift = 0, double rotation = 0,
                         bool folded = true) const;
    struct TransformInputs final
    {
        std::array<double, 2> offset;
        double entryZ = 0, overview = 0;
    };
    TransformInputs transformInputs() const;
    QMatrix4x4 transform(ArchiveCell cell, double lift, double rotation, bool folded,
                         const TransformInputs &inputs, double height, double slope) const;
    struct ExtentCache final
    {
        std::optional<double> lane;
        double extent = 1;
    };
    mutable std::array<ExtentCache, ArchivePool::lanes> m_extentCache;
    mutable double m_extentCenter = 0, m_extentProgress = -1;
    QPointer<OverviewController> m_overview;
    QPointer<MotionController> m_motion;
    ArchivePool m_pool;
    RealColumns m_realColumns;
    ColumnMemory m_memory;
    bool m_realistic = false;
    struct AlbumCoverSource final
    {
        CoverSource cover;
        QString title;
    };
    QHash<QString, AlbumCoverSource> m_coverSources;
    ArchiveCell m_cell, m_origin;
    QVector<ArchiveInstance> m_instances, m_instanceScratch;
    struct PoolObject final
    {
        struct PhysicalCell final
        {
            qint64 lane = 0, row = 0;
            bool operator==(const PhysicalCell &) const = default;
        };
        PhysicalCell physical;
        quint64 objectId = 0;
    };
    // Each complete 9x48 frontier visits every physical residue exactly once.
    // Store its previous identity directly, including across logical coordinate rebases.
    std::array<PoolObject, ArchivePool::capacity> m_poolObjects;
    ReturningCasePool m_returning;
    DetailMotion m_detailMotion;
    CaseClarity m_clarity;
    PointerDragController m_drag;
    bool m_managedDetail = false, m_navigationLift = false;
    ArchiveField m_field;
    ScalarSpring m_lift;
    double m_rotation = 0;
    double m_liftTarget = yalahoo::sceneGeometry::PreviewLift, m_detail = 0;
    std::optional<double> m_openingTime;
    std::optional<double> m_returnY;
    quint64 m_revision = 0, m_objectSerial = ArchivePool::capacity;
    quint64 m_selectedObject = ++m_objectSerial;
};
SelectionController::SelectionController(MotionController *motion, QObject *parent)
    : QObject(parent), d(std::make_unique<Private>(this, motion))
{
    d->m_instances.reserve(ArchivePool::capacity + AdditionalCaseCapacity);
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
    d->m_pool.setColumns(std::move(columns));
    d->m_coverSources.clear();
    if (snapshot)
        d->m_coverSources.reserve(snapshot->albums.size());
    if (snapshot)
        for (const auto &album : snapshot->albums) {
            const auto entry = d->m_coverSources.tryEmplace(album.id);
            if (!entry.inserted)
                continue;
            const auto path = album.coverUrl.scheme() == "qrc"_L1 ? u":"_s + album.coverUrl.path()
                                                                  : album.coverUrl.toLocalFile();
            *entry.iterator = { { path,
                                  graphemePrefix(album.title, MaximumPlaceholderTitleCharacters) },
                                album.title };
        }
    d->m_returning.clear();
    d->m_drag.reset();
    d->m_detailMotion.reset();
    d->m_clarity.select();
    d->m_managedDetail = d->m_navigationLift = false;
    d->m_detail = 0;
    d->m_liftTarget = sceneGeometry::PreviewLift;
    d->m_poolObjects.fill({ });
    d->m_field.clearPulses();
    d->m_origin = { 0, 0 };
    d->m_cell = d->m_pool.location(oldId).value_or(d->m_pool.location(firstId).value_or(
            ArchiveCell{ 0, yalahoo::sceneGeometry::FirstAlbumRow }));
    d->m_realColumns.reset(d->m_pool.rowCounts(), d->m_cell);
    d->m_memory.reset(d->m_pool.columns());
    d->m_lift.finish(d->m_liftTarget);
    d->m_rotation = 0;
    d->m_returnY.reset();
    d->m_selectedObject = ++d->m_objectSerial;
    if (d->m_motion) {
        d->m_motion->travel(-sceneGeometry::ArchiveDepth
                                    - (d->m_cell.row - RailOriginRow) * sceneGeometry::RowSpacing,
                            (d->m_cell.lane - 2) * sceneGeometry::ColumnSpacing, d->m_cell.row,
                            d->m_cell.lane);
        d->m_motion->finish();
    }
    Q_EMIT libraryChanged();
    d->publish();
    Q_EMIT selectionChanged();
}
CoverSource SelectionController::coverSource(const QString &id) const
{
    const auto source = d->m_coverSources.constFind(id);
    return source == d->m_coverSources.cend() ? CoverSource{ } : source->cover;
}
QString SelectionController::albumTitle(const QString &id) const
{
    const auto source = d->m_coverSources.constFind(id);
    return source == d->m_coverSources.cend() ? QString{ } : source->title;
}
QString SelectionController::albumId() const
{
    return d->m_realistic ? d->m_pool.realAlbumAt(d->m_cell) : d->m_pool.albumAt(d->m_cell);
}
bool SelectionController::realistic() const
{
    return d->m_realistic;
}
const RealColumns &SelectionController::realColumns() const
{
    return d->m_realColumns;
}
double SelectionController::browseRow(int lane) const
{
    return d->m_realistic ? d->m_realColumns.row(lane, overviewProgress())
            : d->m_motion
            ? (-d->m_motion->rail() - sceneGeometry::ArchiveDepth) / sceneGeometry::RowSpacing
                    + RailOriginRow
            : d->m_cell.row;
}
void SelectionController::setOverview(OverviewController *overview)
{
    d->m_overview = overview;
}
double SelectionController::overviewProgress() const
{
    return d->m_overview ? d->m_overview->progress() : 0;
}
double SelectionController::centerLane() const
{
    return d->m_motion ? d->m_motion->column() / sceneGeometry::ColumnSpacing + 2 : d->m_cell.lane;
}
double SelectionController::columnExtent(double lane) const
{
    const double progress = overviewProgress();
    if (progress == 0)
        return 1;
    const double center = centerLane();
    if (center != d->m_extentCenter || progress != d->m_extentProgress) {
        d->m_extentCenter = center;
        d->m_extentProgress = progress;
        d->m_extentCache.fill({ });
    }
    auto &entry = d->m_extentCache[ArchivePool::wrap(lane, ArchivePool::lanes)];
    if (entry.lane != lane) {
        entry.lane = lane;
        entry.extent = OverviewController::columnExtent(lane, center, progress);
    }
    return entry.extent;
}
void SelectionController::overviewAlbumClicked()
{
    if (d->m_overview)
        d->m_overview->albumClicked();
}
bool SelectionController::rememberColumnPosition() const
{
    return d->m_memory.enabled;
}
void SelectionController::setRememberColumnPosition(bool enabled)
{
    if (enabled == d->m_memory.enabled)
        return;
    d->m_memory.enabled = enabled;
    if (enabled && d->m_pool.columnCount())
        d->m_memory.remember(
                d->m_pool.columns()[ArchivePool::wrap(d->m_cell.lane, d->m_pool.columnCount())].key,
                albumId());
    Q_EMIT memoryChanged();
}
NavigationAvailability SelectionController::navigationAvailability() const
{
    const auto location = d->m_pool.location(albumId());
    return location
            ? NavigationAvailability::at(d->m_pool.rowCount(location->lane),
                                         int(location->row - yalahoo::sceneGeometry::FirstAlbumRow),
                                         d->m_pool.columnCount(), d->m_realistic)
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
    if (d->m_realistic == realistic || !d->m_motion)
        return;
    const auto canonical = d->m_pool.location(albumId());
    const auto old = d->m_cell;
    const double displayed =
            (-d->m_motion->rail() - sceneGeometry::ArchiveDepth) / sceneGeometry::RowSpacing
            + RailOriginRow;
    d->m_realistic = realistic;
    d->m_cell = canonical.value_or(ArchiveCell{ 0, yalahoo::sceneGeometry::FirstAlbumRow });
    const ArchiveCell shift{ old.lane - d->m_cell.lane, old.row - d->m_cell.row };
    d->m_motion->translate({ shift.row * sceneGeometry::RowSpacing,
                             -shift.lane * sceneGeometry::ColumnSpacing, -shift.row, -shift.lane });
    d->m_realColumns.reset(d->m_pool.rowCounts(), d->m_cell, displayed - shift.row);
    d->m_origin = { 0, 0 };
    d->m_returning.clear();
    d->m_poolObjects.fill({ });
    d->m_field.clearPulses();
    Q_EMIT arrayModeChanged();
    d->publish();
    Q_EMIT selectionChanged();
}
ArchiveCell SelectionController::cell() const
{
    return d->m_cell;
}
ArchiveCell SelectionController::coordinateOrigin() const
{
    return d->m_origin;
}
const ArchivePool &SelectionController::pool() const
{
    return d->m_pool;
}
const QVector<ArchiveInstance> &SelectionController::instances() const
{
    return d->m_instances;
}
quint64 SelectionController::revision() const
{
    return d->m_revision;
}
std::optional<ArchiveCell> SelectionController::Private::selectionCell(const QString &id) const
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
    const auto cell = d->m_pool.location(id.isEmpty() ? albumId() : id);
    return cell
            && NavigationAvailability::at(d->m_pool.rowCount(cell->lane),
                                          int(cell->row - yalahoo::sceneGeometry::FirstAlbumRow),
                                          d->m_pool.columnCount(), d->m_realistic)
                       .allowsRow(direction);
}
QString SelectionController::adjacentAlbum(const QString &id, int direction) const
{
    if (direction != 1 && direction != -1)
        return { };
    const auto cell = d->m_pool.location(id.isEmpty() ? albumId() : id);
    if (!cell || d->m_pool.rowCount(cell->lane) < 2)
        return { };
    const ArchiveCell target{ cell->lane, cell->row + direction };
    return d->m_realistic ? d->m_pool.realAlbumAt(target) : d->m_pool.albumAt(target);
}
bool SelectionController::canSelect(const QString &id) const
{
    const auto cell = d->selectionCell(id);
    return cell
            && (*cell == d->m_cell
                || d->m_returning.canExchange(*cell, d->m_lift.value > InvisibleCaseThreshold));
}
// Resolve the canonical cell and reserve return-copy capacity before changing identity.
// An interrupted selection retains the outgoing displayed pose instead of snapping it back.
bool SelectionController::select(const QString &id)
{
    const auto cell = d->selectionCell(id);
    return cell && selectCell(*cell, true);
}
bool SelectionController::switchDetail(const QString &id)
{
    if (!canSelect(id))
        return false;
    d->m_navigationLift = d->m_detailMotion.placed();
    const bool result = select(id);
    if (result) {
        d->m_drag.reset();
        d->m_clarity.enter(d->m_clarity.value() > AlreadyClearThreshold);
    }
    return result;
}
bool SelectionController::moveRow(int direction)
{
    if (!navigationAvailability().allowsRow(direction))
        return false;
    return selectCell({ d->m_cell.lane, d->m_cell.row + direction });
}
bool SelectionController::moveLane(int direction)
{
    if ((direction != 1 && direction != -1) || !navigationAvailability().left)
        return false;
    // Public selection accepts large periodic coordinates; narrow only after rebase.
    rebase();
    const double lane = d->m_cell.lane + direction;
    return enterColumn(int(lane), false);
}
bool SelectionController::enterColumn(int lane, bool guided)
{
    const auto target = columnTarget(lane, guided);
    return target && selectCell(*target, guided);
}
std::optional<ArchiveCell> SelectionController::columnTarget(int lane, bool guided) const
{
    if (!d->m_motion || !d->m_pool.columnCount())
        return std::nullopt;
    const int canonicalLane = ArchivePool::wrap(lane, d->m_pool.columnCount());
    const auto &column = d->m_pool.columns()[canonicalLane];
    if (column.albumIds.isEmpty())
        return std::nullopt;
    const auto remembered = d->m_pool.location(d->m_memory.recalled(column.key));
    double row;
    if (remembered && remembered->lane == canonicalLane)
        row = d->m_realistic ? remembered->row
                             : ArchivePool::nearestOccurrence(remembered->row, d->m_cell.row,
                                                              d->m_pool.rowCount(canonicalLane));
    else
        row = std::floor(browseRow(canonicalLane) + .5);
    if (d->m_realistic)
        row = std::clamp(row, double(yalahoo::sceneGeometry::FirstAlbumRow),
                         double(LastAlbumRowOffset + column.albumIds.size()));
    const double physicalLane = d->m_realistic ? double(canonicalLane)
            : guided
            ? ArchivePool::nearestOccurrence(
                      canonicalLane, d->m_motion->column() / sceneGeometry::ColumnSpacing + 2,
                      d->m_pool.columnCount())
            : double(lane);
    return ArchiveCell{ physicalLane, row };
}
bool SelectionController::navigateRow(double row)
{
    if (!std::isfinite(row))
        return false;
    // Slider/touch/detail callers share the same finite boundary as directional input.
    if (d->m_realistic
        && (row < yalahoo::sceneGeometry::FirstAlbumRow
            || row >= yalahoo::sceneGeometry::FirstAlbumRow + d->m_pool.rowCount(d->m_cell.lane)))
        return false;
    return selectCell({ d->m_cell.lane, row });
}
bool SelectionController::selectCell(ArchiveCell cell, bool guided)
{
    if (!d->m_motion || !std::isfinite(cell.lane) || !std::isfinite(cell.row)
        || std::abs(cell.lane) > MaximumCoordinateMagnitude
        || std::abs(cell.row) > MaximumCoordinateMagnitude || std::floor(cell.lane) != cell.lane
        || std::floor(cell.row) != cell.row
        || (d->m_realistic ? d->m_pool.realAlbumAt(cell) : d->m_pool.albumAt(cell)).isEmpty())
        return false;
    if (cell == d->m_cell)
        return true;
    if (!d->m_returning.canExchange(cell, d->m_lift.value > InvisibleCaseThreshold))
        return false;
    auto previous = d->m_returning.take(cell);
    if (d->m_lift.value > InvisibleCaseThreshold)
        d->m_returning.append(
                { d->m_cell, albumId(), d->m_lift, d->m_rotation,
                  d->m_rotation != 0 ? std::optional<double>(selectedPosition().y) : std::nullopt,
                  d->m_selectedObject, d->m_clarity.value() });
    const auto old = d->m_cell;
    if (d->m_managedDetail) {
        if (!d->m_detailMotion.placed())
            d->m_navigationLift = false;
        d->m_detailMotion.selectionChanged();
    }
    d->m_drag.reset();
    d->m_cell = cell;
    d->m_memory.remember(
            d->m_pool.columns()[ArchivePool::wrap(cell.lane, d->m_pool.columnCount())].key,
            albumId());
    if (d->m_realistic)
        d->m_realColumns.select(cell, guided,
                                d->m_managedDetail ? d->m_detailMotion.placed()
                                                   : d->m_detail > DetailPlacementThreshold);
    d->m_field.interact();
    d->m_lift = previous ? previous->lift : ScalarSpring{ };
    d->m_rotation = previous ? previous->rotation : 0;
    d->m_returnY = previous ? previous->returnY : std::nullopt;
    d->m_selectedObject = previous ? previous->objectId : ++d->m_objectSerial;
    d->m_clarity.select(previous ? previous->clarity : 0);
    const ArchiveValues targets{ -sceneGeometry::ArchiveDepth
                                         - (cell.row - RailOriginRow) * sceneGeometry::RowSpacing,
                                 (cell.lane - 2) * sceneGeometry::ColumnSpacing, cell.row,
                                 cell.lane };
    const bool distant = cell.lane != old.lane && std::abs(cell.row - old.row) > 1;
    const bool detailNavigation =
            d->m_managedDetail && d->m_navigationLift && d->m_detailMotion.placed();
    if (detailNavigation)
        d->m_motion->cancelTravel();
    else if (guided || distant || d->m_motion->moving())
        d->m_motion->retarget(targets);
    if (!detailNavigation)
        d->m_field.pulse(cell);
    d->publish();
    Q_EMIT selectionChanged();
    return true;
}
void SelectionController::setLift(double lift)
{
    if (!std::isfinite(lift) || lift < 0 || lift > InspectionLift)
        return;
    d->m_liftTarget = lift;
    d->m_lift.finish(lift);
    d->publish();
}
void SelectionController::setRotation(double radians)
{
    if (!std::isfinite(radians))
        return;
    d->m_rotation = radians;
    d->publish();
}
void SelectionController::setDetail(double detail)
{
    if (!std::isfinite(detail))
        return;
    d->m_detail = std::clamp(detail, 0.0, 1.0);
    // Frozen music scene: MUSIC_MODEL.height + 0.12 clears the shelf.
    setLift(sceneGeometry::PreviewLift
            + (sceneGeometry::CaseHeight + yalahoo::sceneGeometry::InspectionLiftPadding
               - sceneGeometry::PreviewLift)
                    * d->m_detail);
}
void SelectionController::returnSelected()
{
    d->m_returnY = d->m_rotation != 0 ? std::optional<double>(selectedPosition().y) : std::nullopt;
    d->m_liftTarget = 0;
}
void SelectionController::requestDetail(bool detail)
{
    d->m_field.interact();
    d->m_managedDetail = true;
    d->m_detailMotion.request(detail);
    d->m_drag.cancel();
    if (detail) {
        d->m_clarity.enter();
        d->m_returnY.reset();
    } else {
        d->m_clarity.leave();
        if (d->m_rotation != 0)
            d->m_returnY = selectedPosition().y;
    }
    if (d->m_motion && d->m_motion->reducedMotion())
        advance(0);
}
std::array<double, 2> SelectionController::navigationSpeeds() const
{
    return d->m_motion ? std::array<double, 2>{ d->m_motion->springs()[ColumnAxis].velocity
                                                        / sceneGeometry::ColumnSpacing,
                                                d->m_motion->springs()[RailAxis].velocity
                                                        / sceneGeometry::RowSpacing }
                       : std::array<double, 2>{ 0, 0 };
}
bool SelectionController::Private::tracksSettled() const
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
    if (!d->m_managedDetail)
        return;
    const auto &detail = d->m_detailMotion;
    const double target = detail.holdsDetail() ? 1 : 0;
    const bool liftSettled = detail.settled() && d->tracksSettled()
            && std::abs(d->m_detail - target) < AlignmentTolerance
            && std::abs(d->m_lift.value
                        - (target ? sceneGeometry::InspectionLift : sceneGeometry::PreviewLift))
                    < TrackPositionTolerance
            && std::abs(d->m_rotation) < AlignmentTolerance
            && std::abs(d->m_lift.velocity) < TrackVelocityTolerance;
    d->m_detailMotion.updateReadiness(dt, cameraSettled, liftSettled, d->m_motion->reducedMotion());
    if (d->m_detailMotion.phase() == DetailMotion::Phase::Archive)
        d->m_navigationLift = false;
}
bool SelectionController::archiveReady() const
{
    return d->tracksSettled()
            && (!d->m_managedDetail || d->m_detailMotion.phase() == DetailMotion::Phase::Archive);
}
bool SelectionController::archiveInteractive() const
{
    return archiveReady()
            || (d->m_detailMotion.phase() == DetailMotion::Phase::ReturningArray
                && d->m_detailMotion.settled() && d->m_detail < AlignmentTolerance
                && std::abs(d->m_rotation) < AlignmentTolerance);
}
bool SelectionController::canInspect() const
{
    if (!d->m_managedDetail || d->m_detailMotion.phase() != DetailMotion::Phase::Presented
        || d->m_detail <= DetailOcclusionProgress || d->m_field.pulseGain() >= ActivityThreshold)
        return false;
    const auto top = [this](ArchiveCell cell, double lift = 0) {
        return -sceneGeometry::FloorDepth + d->field(cell.row, cell.lane) + lift
                + sceneGeometry::CaseTop;
    };
    double neighborTop = top({ d->m_cell.lane, d->m_cell.row - NeighborRadiusRows });
    for (int row = -NeighborRadiusRows + 1; row <= NeighborRadiusRows; ++row)
        if (row != 0)
            neighborTop = std::max(neighborTop, top({ d->m_cell.lane, d->m_cell.row + row }));
    for (const auto &copy : d->m_returning)
        if (copy.cell.lane == d->m_cell.lane
            && std::abs(copy.cell.row - d->m_cell.row) <= NeighborRadiusRows)
            neighborTop = std::max(neighborTop, top(copy.cell, copy.lift.value));
    return selectedPosition().y + sceneGeometry::CaseBottom - neighborTop > NeighborClearance;
}
bool SelectionController::beginDrag(QPointF point, int pointerId)
{
    if (!canInspect())
        return false;
    return d->m_drag.press(point, pointerId);
}
bool SelectionController::moveDrag(QPointF point, int pointerId)
{
    if (!canInspect()) {
        d->m_drag.cancel();
        return false;
    }
    return d->m_drag.move(point, pointerId);
}
void SelectionController::endDrag(QPointF point, int pointerId)
{
    d->m_drag.release(point, pointerId);
}
void SelectionController::cancelDrag()
{
    d->m_drag.cancel();
}
ArchiveField::Column SelectionController::Private::fieldColumn(double lane) const
{
    ArchiveField::Column column;
    column.lane = lane;
    column.reduced = m_motion->reducedMotion();
    if (m_realistic) {
        column.activity = m_realColumns.activity(int(lane));
        column.center = m_realColumns.row(int(lane), m_openingTime ? 0 : q->overviewProgress());
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
double SelectionController::field(double row, double lane) const
{
    return d->field(row, lane);
}
double SelectionController::Private::field(double row, double lane) const
{
    return m_motion ? field(row, fieldColumn(lane)) : 0;
}
double SelectionController::Private::field(double row, const ArchiveField::Column &column) const
{
    return m_field.sample(row, column, { m_realistic, m_openingTime, m_cell, m_origin });
}
QMatrix4x4 SelectionController::Private::transform(ArchiveCell cell, double lift, double rotation,
                                                   bool folded) const
{
    QMatrix4x4 matrix;
    if (!m_motion)
        return matrix;
    return transform(cell, lift, rotation, folded, transformInputs(), field(cell.row, cell.lane),
                     field(cell.row + .5, cell.lane) - field(cell.row - .5, cell.lane));
}
SelectionController::Private::TransformInputs SelectionController::Private::transformInputs() const
{
    const double entryZ = m_openingTime ? -OpeningEntryDepth
                    * std::pow(1
                                       - std::clamp((*m_openingTime
                                                     - yalahoo::openingTimeline::SceneOriginSeconds)
                                                            / OpeningEntrySeconds,
                                                    0.0, 1.0),
                               2)
                                        : 0;
    return { displayOffset(), entryZ, q->overviewProgress() };
}
QMatrix4x4 SelectionController::Private::transform(ArchiveCell cell, double lift, double rotation,
                                                   bool folded,
                                                   const Private::TransformInputs &inputs,
                                                   double height, double slope) const
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
    const double extent = folded ? q->columnExtent(cell.lane) : 1;
    if (extent <= 0) {
        matrix.scale(0);
    } else if (extent != 1) {
        matrix.translate(0, float(sceneGeometry::CaseBottom * (1 - extent)), 0);
        matrix.scale(1, float(extent), 1);
    }
    return matrix;
}
std::array<double, 2> SelectionController::Private::displayOffset() const
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
    const auto p =
            d->transform(d->m_cell, d->m_lift.value, d->m_rotation).column(TranslationColumn);
    return { p.x(), p.y(), p.z() };
}
QQuaternion SelectionController::selectedRotation() const
{
    return QQuaternion::fromRotationMatrix(
            d->transform(d->m_cell, d->m_lift.value, d->m_rotation, false).normalMatrix());
}
// Translate displayed tracks and remembered origins by the same whole-period offset.
// This bounds floating-point coordinates without changing the visible pose or canonical album ID.
ArchiveCell SelectionController::rebase()
{
    ArchiveCell shift{ 0, 0 };
    if (!d->m_motion || d->m_realistic)
        return shift;
    const auto period = d->m_pool.rowPeriod();
    const int lanes = qMax(1, d->m_pool.columnCount());
    if (std::abs(d->m_cell.lane) > RebaseCoordinateMagnitude)
        shift.lane = std::floor((d->m_cell.lane - 2) / lanes + .5) * lanes;
    if (period && std::abs(d->m_cell.row) > std::max(RebaseRowMagnitude, double(*period) * 2))
        shift.row = std::floor((d->m_cell.row - yalahoo::sceneGeometry::FirstAlbumRow) / *period)
                * *period;
    if (shift == ArchiveCell{ 0, 0 })
        return shift;
    d->m_cell.lane -= shift.lane;
    d->m_cell.row -= shift.row;
    d->m_origin.lane += shift.lane;
    d->m_origin.row += shift.row;
    for (auto &copy : d->m_returning) {
        copy.cell.lane -= shift.lane;
        copy.cell.row -= shift.row;
    }
    d->m_field.rebase(shift);
    d->m_motion->translate({ shift.row * sceneGeometry::RowSpacing,
                             -shift.lane * sceneGeometry::ColumnSpacing, -shift.row, -shift.lane });
    Q_EMIT coordinatesRebased(shift.lane, shift.row);
    d->publish();
    return shift;
}
void SelectionController::setOpeningTime(std::optional<double> time)
{
    if (time && !std::isfinite(*time))
        return;
    const bool entering = time && !d->m_openingTime;
    d->m_openingTime = time;
    d->m_field.beginOpening();
    if (time) {
        d->m_returning.clear();
        d->m_rotation = d->m_detail = 0;
        d->m_returnY.reset();
        d->m_managedDetail = d->m_navigationLift = false;
        d->m_detailMotion.reset();
        d->m_clarity.select();
        const double intro = smooth((*time - yalahoo::openingTimeline::PreviewStartSeconds)
                                    / yalahoo::openingTimeline::PreviewDurationSeconds);
        d->m_lift.finish(extraction(*time) * (1 - intro) + sceneGeometry::PreviewLift * intro);
        d->m_liftTarget = sceneGeometry::PreviewLift;
        if (entering && d->m_motion) {
            d->m_motion->travel(-sceneGeometry::ArchiveDepth
                                        - (d->m_cell.row - RailOriginRow)
                                                * sceneGeometry::RowSpacing,
                                (d->m_cell.lane - 2) * sceneGeometry::ColumnSpacing, d->m_cell.row,
                                d->m_cell.lane);
            d->m_motion->finish();
        }
        d->m_realColumns.update(0, true);
    }
    d->publish();
}
void SelectionController::Private::advanceLift(double dt, bool reduced, bool detailNavigation)
{
    m_returnY.reset();
    const bool aligningNeighbor =
            std::any_of(m_returning.cbegin(), m_returning.cend(), [&](const ReturningCase &copy) {
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
    if (d->m_openingTime)
        return;
    if (!d->m_motion || d->m_pool.columnCount() == 0 || !std::isfinite(dt) || dt < 0)
        return;
    rebase();
    const bool reduced = d->m_motion->reducedMotion();
    const ArchiveValues targets{
        -sceneGeometry::ArchiveDepth - (d->m_cell.row - RailOriginRow) * sceneGeometry::RowSpacing,
        (d->m_cell.lane - 2) * sceneGeometry::ColumnSpacing, d->m_cell.row, d->m_cell.lane
    };
    const bool detailNavigation =
            d->m_managedDetail && d->m_navigationLift && d->m_detailMotion.placed();
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
    if (!d->m_motion->moving()) {
        auto springs = d->m_motion->springs();
        for (int i = 0; i < ArchiveAxisCount; ++i) {
            if (reduced)
                springs[i].finish(targets[i]);
            else
                springs[i].update(targets[i], rates[i], dt);
        }
        d->m_motion->observe(springs, targets, rates);
    }
    if (d->m_realistic)
        d->m_realColumns.update(dt, reduced);
    const bool aligning = std::any_of(d->m_returning.cbegin(), d->m_returning.cend(),
                                      [](const ReturningCase &copy) {
                                          return copy.returnY.has_value();
                                      });
    const bool idleEligible =
            d->m_liftTarget > 0 && d->m_detail < ActivityThreshold && !d->m_returnY && !aligning;
    d->m_field.advance(dt, reduced, idleEligible, d->m_detail > 0 || d->m_returnY || aligning);
    const bool inspecting = d->m_managedDetail && d->m_detailMotion.placed() && !d->m_returnY;
    d->m_rotation = d->m_drag.advance(d->m_rotation, dt, inspecting, reduced);
    if (d->m_managedDetail) {
        d->m_detailMotion.advance(dt, reduced, d->m_rotation == 0);
        d->m_detail = d->m_detailMotion.value();
    }
    const double base = -sceneGeometry::FloorDepth + d->field(d->m_cell.row, d->m_cell.lane);
    if (d->m_returnY && d->m_rotation != 0)
        d->m_lift.finish(*d->m_returnY - base);
    else {
        d->advanceLift(dt, reduced, detailNavigation);
    }
    if (d->m_managedDetail)
        d->m_clarity.advance(
                dt, d->m_detail > ClarityReadyProgress && d->m_lift.value > ClarityReadyLift,
                reduced);
    for (auto &copy : d->m_returning) {
        copy.clarity = reduced ? 0 : copy.clarity * std::exp(-dt * ReturnClarityDecayRate);
        copy.rotation = reduced ? 0 : returnStep(copy.rotation, dt, false);
        if (copy.returnY && !reduced) {
            copy.lift.finish(*copy.returnY + sceneGeometry::FloorDepth
                             - d->field(copy.cell.row, copy.cell.lane));
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
    d->m_returning.removeIf([](const ReturningCase &copy) {
        return copy.lift.value < InvisibleCaseThreshold
                && std::abs(copy.rotation) < InvisibleCaseThreshold;
    });
    d->publish();
}
void SelectionController::Private::publish()
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
                    q->overviewProgress());
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
    std::optional<ArchiveField::Column> column;
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
                || std::any_of(m_returning.cbegin(), m_returning.cend(),
                               [&](const ReturningCase &copy) {
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
    if (!q->albumId().isEmpty())
        next.append({ m_cell, q->albumId(), m_selectedObject,
                      transform(m_cell, m_lift.value, m_rotation), true, m_clarity.value() });
    if (next == m_instances)
        return;
    m_instances.swap(next);
    ++m_revision;
    Q_EMIT q->frameChanged();
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
    const ArchiveInstance *nearestInstance = nullptr;
    QVector3D nearestLocal;
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
        nearestInstance = &instance;
        nearestLocal = localStart + delta * float(*enter);
    }
    if (!nearestInstance)
        return { };
    // Only the winning intersection needs a QML map and a world-space conversion.
    const auto &hit = *nearestInstance;
    return { { u"albumId"_s, hit.albumId },
             { u"objectId"_s, QVariant::fromValue(hit.objectId) },
             { u"lane"_s, hit.cell.lane },
             { u"row"_s, hit.cell.row },
             { u"localPoint"_s, QVariant::fromValue(nearestLocal) },
             { u"worldPoint"_s, QVariant::fromValue(hit.transform.map(nearestLocal)) },
             { u"frameRevision"_s, QVariant::fromValue(frame.revision) } };
}
double SelectionController::selectedClarity() const
{
    return d->m_clarity.value();
}
const DetailMotion &SelectionController::detailMotion() const
{
    return d->m_detailMotion;
}
bool SelectionController::dragging() const
{
    return d->m_drag.dragging();
}
double SelectionController::rotation() const
{
    return d->m_rotation;
}
Kinematics SelectionController::liftState() const
{
    return d->m_lift;
}
const ReturningCasePool &SelectionController::returningCases() const
{
    return d->m_returning;
}
SelectionController::~SelectionController() = default;

}
