#include "SelectionController.h"
#include "OverviewController.h"
#include "MotionFunctions.h"
#include <QVector4D>
#include <algorithm>
#include <cmath>
#include <limits>

namespace yalahoo {
SelectionController::SelectionController(MotionController *motion, QObject *parent)
    : QObject(parent), m_motion(motion)
{
    m_instances.reserve(ArchivePool::capacity + 4);
    if (motion) {
        connect(motion->clock(), &FrameClock::frame, this, [this](double, double dt) {
            advance(dt);
        });
        connect(motion, &MotionController::reducedMotionChanged, this, [this] {
            advance(0);
        });
    }
}
void SelectionController::setColumns(QList<LibraryColumn> columns, Snapshot snapshot)
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
        for (const auto &album : snapshot->albums) {
            const auto path = album.coverUrl.scheme() == QLatin1String("qrc")
                    ? QStringLiteral(":") + album.coverUrl.path()
                    : album.coverUrl.toLocalFile();
            if (!m_coverSources.contains(album.id))
                m_coverSources.insert(album.id, { path, album.title.left(256) });
        }
    m_returning.clear();
    m_drag.reset();
    m_detailMotion.reset();
    m_clarity.select();
    m_managedDetail = m_navigationLift = false;
    m_detail = 0;
    m_liftTarget = .9;
    m_poolObjects.clear();
    m_pulses.clear();
    m_origin = { 0, 0 };
    m_cell = m_pool.location(oldId).value_or(
            m_pool.location(firstId).value_or(ArchiveCell{ 0, 12 }));
    QList<int> counts;
    for (const auto &column : m_pool.columns())
        counts.append(int(column.albumIds.size()));
    m_realColumns.reset(counts, m_cell);
    m_memory.reset(m_pool.columns());
    m_lift.finish(m_liftTarget);
    m_rotation = 0;
    m_returnY.reset();
    m_selectedObject = ++m_objectSerial;
    if (m_motion) {
        m_motion->travel(-2.17 - (m_cell.row - 15.5) * .62, (m_cell.lane - 2) * 5.2, m_cell.row,
                         m_cell.lane);
        m_motion->finish();
    }
    emit libraryChanged();
    publish();
    emit selectionChanged();
}
QPair<QString, QString> SelectionController::coverSource(const QString &id) const
{
    return m_coverSources.value(id);
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
            : m_motion ? (-m_motion->rail() - 2.17) / .62 + 15.5
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
    return m_motion ? m_motion->column() / 5.2 + 2 : m_cell.lane;
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
    emit memoryChanged();
}
NavigationAvailability SelectionController::navigationAvailability() const
{
    const auto location = m_pool.location(albumId());
    return location
            ? NavigationAvailability::at(m_pool.rowCount(location->lane), int(location->row - 12),
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
    const double displayed = (-m_motion->rail() - 2.17) / .62 + 15.5;
    m_realistic = realistic;
    m_cell = canonical.value_or(ArchiveCell{ 0, 12 });
    const ArchiveCell shift{ old.lane - m_cell.lane, old.row - m_cell.row };
    m_motion->translate({ shift.row * .62, -shift.lane * 5.2, -shift.row, -shift.lane });
    QList<int> counts;
    for (const auto &column : m_pool.columns())
        counts.append(int(column.albumIds.size()));
    m_realColumns.reset(counts, m_cell, displayed - shift.row);
    m_origin = { 0, 0 };
    m_returning.clear();
    m_poolObjects.clear();
    m_pulses.clear();
    emit arrayModeChanged();
    publish();
    emit selectionChanged();
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
            && NavigationAvailability::at(m_pool.rowCount(cell->lane), int(cell->row - 12),
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
    return cell && (*cell == m_cell || m_returning.canExchange(*cell, m_lift.value > .0001));
}
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
        m_clarity.enter(m_clarity.value() > .999);
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
        row = std::clamp(row, 12.0, double(11 + column.albumIds.size()));
    const double physicalLane = m_realistic ? double(canonicalLane)
            : guided ? ArchivePool::nearestOccurrence(canonicalLane, m_motion->column() / 5.2 + 2,
                                                      m_pool.columnCount())
                     : double(lane);
    return ArchiveCell{ physicalLane, row };
}
bool SelectionController::navigateRow(double row)
{
    if (!std::isfinite(row))
        return false;
    // Slider/touch/detail callers share the same finite boundary as directional input.
    if (m_realistic && (row < 12 || row >= 12 + m_pool.rowCount(m_cell.lane)))
        return false;
    return selectCell({ m_cell.lane, row });
}
bool SelectionController::selectCell(ArchiveCell cell, bool guided)
{
    if (!m_motion || !std::isfinite(cell.lane) || !std::isfinite(cell.row)
        || std::abs(cell.lane) > 1e12 || std::abs(cell.row) > 1e12
        || std::floor(cell.lane) != cell.lane || std::floor(cell.row) != cell.row
        || (m_realistic ? m_pool.realAlbumAt(cell) : m_pool.albumAt(cell)).isEmpty())
        return false;
    if (cell == m_cell)
        return true;
    if (!m_returning.canExchange(cell, m_lift.value > .0001))
        return false;
    auto previous = m_returning.take(cell);
    if (m_lift.value > .0001)
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
                             m_managedDetail ? m_detailMotion.placed() : m_detail > .99);
    m_lastInteraction = m_time;
    m_lift = previous ? previous->lift : ScalarSpring{ };
    m_rotation = previous ? previous->rotation : 0;
    m_returnY = previous ? previous->returnY : std::nullopt;
    m_selectedObject = previous ? previous->objectId : ++m_objectSerial;
    m_clarity.select(previous ? previous->clarity : 0);
    const ArchiveValues targets{ -2.17 - (cell.row - 15.5) * .62, (cell.lane - 2) * 5.2, cell.row,
                                 cell.lane };
    const bool distant = cell.lane != old.lane && std::abs(cell.row - old.row) > 1;
    const bool detailNavigation = m_managedDetail && m_navigationLift && m_detailMotion.placed();
    if (detailNavigation)
        m_motion->cancelTravel();
    else if (guided || distant || m_motion->moving())
        m_motion->retarget(targets);
    if (!detailNavigation)
        m_pulses.append({ cell, m_time });
    if (m_pulses.size() > 6)
        m_pulses.removeFirst();
    publish();
    emit selectionChanged();
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
    setLift(.9 + (3.35 + .12 - .9) * m_detail);
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
    return m_motion ? std::array<double, 2>{ m_motion->springs()[1].velocity / 5.2,
                                             m_motion->springs()[0].velocity / .62 }
                    : std::array<double, 2>{ 0, 0 };
}
bool SelectionController::tracksSettled() const
{
    if (!m_motion || m_motion->moving())
        return false;
    const ArchiveValues targets{ -2.17 - (m_cell.row - 15.5) * .62, (m_cell.lane - 2) * 5.2,
                                 m_cell.row, m_cell.lane };
    for (size_t i = 0; i < targets.size(); ++i)
        if (std::abs(m_motion->springs()[i].value - targets[i]) >= .008
            || std::abs(m_motion->springs()[i].velocity) >= .025)
            return false;
    return !m_realistic || m_realColumns.settled(int(m_cell.lane));
}
void SelectionController::updateDetailReadiness(double dt, bool cameraSettled)
{
    if (!m_managedDetail)
        return;
    const auto &d = m_detailMotion;
    const double target = d.holdsDetail() ? 1 : 0;
    const bool liftSettled = d.settled() && tracksSettled() && std::abs(m_detail - target) < .001
            && std::abs(m_lift.value - (target ? 3.47 : .9)) < .008 && std::abs(m_rotation) < .001
            && std::abs(m_lift.velocity) < .025;
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
                && m_detailMotion.settled() && m_detail < .001 && std::abs(m_rotation) < .001);
}
bool SelectionController::canInspect() const
{
    if (!m_managedDetail || m_detailMotion.phase() != DetailMotion::Phase::Presented
        || m_detail <= .9 || m_pulseGain >= .01)
        return false;
    double neighborTop = -std::numeric_limits<double>::infinity();
    for (int row = -5; row <= 5; ++row)
        if (row != 0)
            neighborTop =
                    std::max(neighborTop, -4.6 + field(m_cell.row + row, m_cell.lane) + 3.525);
    for (const auto &copy : m_returning)
        if (copy.cell.lane == m_cell.lane && std::abs(copy.cell.row - m_cell.row) <= 5)
            neighborTop =
                    std::max(neighborTop,
                             -4.6 + field(copy.cell.row, copy.cell.lane) + copy.lift.value + 3.525);
    return selectedPosition().y + .175 - neighborTop > .05;
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
double SelectionController::field(double row, double lane) const
{
    if (!m_motion)
        return 0;
    const bool reduced = m_motion->reducedMotion();
    if (m_realistic) {
        const double activity = m_realColumns.activity(int(lane));
        double height =
                reduced ? 0 : idleWave(row, lane, m_time) * (1 + (m_idleGain - 1) * activity);
        if (!reduced && activity > 0) {
            double ripple = 0;
            for (const auto &pulse : m_pulses)
                if (pulse.cell.lane == lane)
                    ripple +=
                            musicSelectionWave(std::abs(row - pulse.cell.row), m_time - pulse.time);
            height += std::clamp(ripple, -.24, .24) * m_pulseGain * activity;
        }
        return height
                + settlingWave(row - m_realColumns.row(int(lane), overviewProgress()), 26.56)
                * activity;
    }
    double height = 0;
    if (m_scanBlend != 0)
        height += archiveWave(row + m_origin.row, lane + m_origin.lane, m_scanTime) * m_scanBlend;
    if (m_idleGain != 0)
        height += idleWave(row + m_origin.row, lane + m_origin.lane, m_time) * m_idleGain;
    if (!reduced && m_pulseGain != 0) {
        double ripple = 0;
        for (const auto &pulse : m_pulses)
            ripple += musicSelectionWave(
                    std::hypot(row - pulse.cell.row, (lane - pulse.cell.lane) * 2.2),
                    m_time - pulse.time);
        height += std::clamp(ripple, -.24, .24) * m_pulseGain;
    }
    const auto &springs = m_motion->springs();
    return height
            + settlingWave(row - springs[2].value, 26.56) * columnStrength(lane, springs[3].value);
}
QMatrix4x4 SelectionController::transform(ArchiveCell cell, double lift, double rotation,
                                          bool folded) const
{
    QMatrix4x4 matrix;
    if (!m_motion)
        return matrix;
    const double slope = field(cell.row + .5, cell.lane) - field(cell.row - .5, cell.lane);
    const auto offset = displayOffset();
    matrix.translate(
            float((cell.lane - m_cell.lane) * 5.2 - offset[1]),
            float(-4.6 + field(cell.row, cell.lane) + lift),
            float(m_realistic
                          ? (cell.row - m_realColumns.row(int(cell.lane), overviewProgress())) * .62
                                  - 2.17
                          : (cell.row - m_cell.row) * .62 - 2.17 + offset[0]));
    const double quality = smooth(lift / .4);
    const double pitch = slope * .024 * (1 - m_detail) * (1 - quality);
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
        matrix.translate(0, float(.175 * (1 - extent)), 0);
        matrix.scale(1, float(extent), 1);
    }
    return matrix;
}
std::array<double, 2> SelectionController::displayOffset() const
{
    const auto &s = m_motion->springs();
    const std::array<double, 2> target{ -2.17 - (m_cell.row - 15.5) * .62,
                                        (m_cell.lane - 2) * 5.2 };
    std::array<double, 2> offset;
    for (int i = 0; i < 2; ++i) {
        offset[i] = s[i].value - target[i];
        // Translate can accumulate a few ulps. Preserve exact integer recycle ties at rest.
        const double roundoff = 8 * std::numeric_limits<double>::epsilon()
                * std::max({ 1.0, std::abs(s[i].value), std::abs(target[i]),
                             std::abs(target[i])
                                     + (i == 0 ? std::abs(m_origin.row * .62)
                                               : std::abs(m_origin.lane * 5.2)) });
        if (!m_motion->moving() && std::abs(offset[i]) <= roundoff)
            offset[i] = 0;
    }
    return offset;
}
SceneVector SelectionController::selectedPosition() const
{
    const auto p = transform(m_cell, m_lift.value, m_rotation).column(3);
    return { p.x(), p.y(), p.z() };
}
QQuaternion SelectionController::selectedRotation() const
{
    return QQuaternion::fromRotationMatrix(
            transform(m_cell, m_lift.value, m_rotation, false).normalMatrix());
}
ArchiveCell SelectionController::rebase()
{
    ArchiveCell shift{ 0, 0 };
    if (!m_motion || m_realistic)
        return shift;
    const double period = m_pool.rowPeriod();
    const int lanes = qMax(1, m_pool.columnCount());
    if (std::abs(m_cell.lane) > 2048)
        shift.lane = std::floor((m_cell.lane - 2) / lanes + .5) * lanes;
    if (std::abs(m_cell.row) > std::max(2048.0, period * 2))
        shift.row = std::floor((m_cell.row - 12) / period) * period;
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
    m_motion->translate({ shift.row * .62, -shift.lane * 5.2, -shift.row, -shift.lane });
    emit coordinatesRebased(shift.lane, shift.row);
    publish();
    return shift;
}
void SelectionController::advance(double dt)
{
    if (!m_motion || m_pool.columnCount() == 0 || !std::isfinite(dt) || dt < 0)
        return;
    rebase();
    const bool reduced = m_motion->reducedMotion();
    const ArchiveValues targets{ -2.17 - (m_cell.row - 15.5) * .62, (m_cell.lane - 2) * 5.2,
                                 m_cell.row, m_cell.lane };
    const bool detailNavigation = m_managedDetail && m_navigationLift && m_detailMotion.placed();
    const ArchiveValues rates{ reduced ? 35.0
                                       : detailNavigation ? 9.0
                                                          : 3.7,
                               reduced ? 35.0
                                       : detailNavigation ? 9.0
                                                          : 3.7,
                               reduced ? 35.0
                                       : detailNavigation ? 9.0
                                                          : 5.0,
                               reduced ? 35.0
                                       : detailNavigation ? 9.0
                                                          : 4.0 };
    if (!m_motion->moving()) {
        auto springs = m_motion->springs();
        for (int i = 0; i < 4; ++i) {
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
    m_scanBlend *= std::exp(-dt * 3);
    m_pulses.removeIf([&](const Pulse &p) {
        return m_time - p.time >= 3.2;
    });
    const bool aligning =
            std::any_of(m_returning.cbegin(), m_returning.cend(), [](const Copy &copy) {
                return copy.returnY.has_value();
            });
    const bool idle = !reduced && m_liftTarget > 0 && m_detail < .01 && !m_returnY && !aligning
            && m_time - m_lastInteraction > 2.5;
    m_idleGain += ((idle ? 1 : 0) - m_idleGain) * (1 - std::exp(-dt * (idle ? .8 : 4)));
    const bool mutePulse = m_detail > 0 || m_returnY || aligning;
    m_pulseGain += ((mutePulse ? 0 : 1) - m_pulseGain) * (1 - std::exp(-dt * 8));
    if (reduced)
        m_idleGain = 0;
    const bool inspecting = m_managedDetail && m_detailMotion.placed() && !m_returnY;
    m_rotation = m_drag.advance(m_rotation, dt, inspecting, reduced);
    if (m_managedDetail) {
        m_detailMotion.advance(dt, reduced, m_rotation == 0);
        m_detail = m_detailMotion.value();
    }
    const double base = -4.6 + field(m_cell.row, m_cell.lane);
    if (m_returnY && m_rotation != 0)
        m_lift.finish(*m_returnY - base);
    else {
        m_returnY.reset();
        const bool aligningNeighbor =
                std::any_of(m_returning.cbegin(), m_returning.cend(), [&](const Copy &copy) {
                    return copy.returnY && copy.cell.lane == m_cell.lane
                            && std::abs(copy.cell.row - m_cell.row) < 5;
                });
        if (m_managedDetail && m_navigationLift) {
            m_liftTarget = m_detailMotion.placed() ? (!reduced && aligningNeighbor ? 0 : 3.47) : .9;
            if (reduced)
                m_lift.finish(m_liftTarget);
            else
                m_lift.update(m_liftTarget, detailNavigation ? 9 : 4.2, dt);
        } else if (m_managedDetail
                   && (m_detail > 0 || !m_detailMotion.settled() || m_detailMotion.holdsDetail())) {
            m_lift.value = .9 + (3.47 - .9) * m_detail;
            m_lift.velocity = m_detailMotion.state().value == m_detail
                    ? (3.47 - .9) * m_detailMotion.state().velocity
                    : 0;
        } else if (reduced)
            m_lift.finish(m_liftTarget);
        else
            m_lift.update(m_liftTarget, 4.2, dt);
    }
    if (m_managedDetail)
        m_clarity.advance(dt, m_detail > .78 && m_lift.value > 3.3, reduced);
    for (auto &copy : m_returning) {
        copy.clarity = reduced ? 0 : copy.clarity * std::exp(-dt * 9);
        copy.rotation = reduced ? 0 : returnStep(copy.rotation, dt, false);
        if (copy.returnY && !reduced) {
            copy.lift.finish(*copy.returnY + 4.6 - field(copy.cell.row, copy.cell.lane));
            if (copy.rotation == 0)
                copy.returnY.reset();
        } else {
            copy.returnY.reset();
            if (reduced)
                copy.lift.finish(0);
            else
                copy.lift.update(0, detailNavigation ? 9 : 4.5, dt);
        }
    }
    m_returning.removeIf([](const Copy &copy) {
        return copy.lift.value < .0001 && std::abs(copy.rotation) < .0001;
    });
    publish();
}
void SelectionController::publish()
{
    if (!m_motion)
        return;
    const auto offset = displayOffset();
    const ArchiveCell center{ m_cell.lane + offset[1] / 5.2, m_cell.row - offset[0] / .62 };
    std::array<double, ArchivePool::lanes> rows;
    if (m_realistic)
        for (int index = 0; index < ArchivePool::lanes; ++index)
            rows[index] = m_realColumns.row(
                    int(ArchivePool::visibleCell(index * ArchivePool::rows, center).lane),
                    overviewProgress());
    m_pool.update(center, m_realistic ? &rows : nullptr);
    QVector<ArchiveInstance> next;
    next.reserve(ArchivePool::capacity + 4);
    QHash<QPair<qint64, qint64>, quint64> objects;
    objects.reserve(ArchivePool::capacity);
    for (auto instance : m_pool.instances()) {
        // Optical depth ties use the physical identity, which survives logical rebase.
        const auto physical = qMakePair(qint64(instance.cell.lane + m_origin.lane),
                                        qint64(instance.cell.row + m_origin.row));
        instance.objectId = m_poolObjects.value(physical, 0);
        if (!instance.objectId)
            instance.objectId = ++m_objectSerial;
        objects.insert(physical, instance.objectId);
        const bool hidden = instance.cell == m_cell
                || std::any_of(m_returning.cbegin(), m_returning.cend(), [&](const Copy &copy) {
                                return copy.cell == instance.cell;
                            });
        if (hidden || instance.albumId.isEmpty())
            continue;
        instance.transform = transform(instance.cell);
        next.append(std::move(instance));
    }
    m_poolObjects = std::move(objects);
    for (const auto &copy : m_returning)
        next.append({ copy.cell, copy.albumId, copy.objectId,
                      transform(copy.cell, copy.lift.value, copy.rotation), true, copy.clarity });
    if (!albumId().isEmpty())
        next.append({ m_cell, albumId(), m_selectedObject,
                      transform(m_cell, m_lift.value, m_rotation), true, m_clarity.value() });
    if (next == m_instances)
        return;
    m_instances = std::move(next);
    ++m_revision;
    emit frameChanged();
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
        if (columnExtent(instance.cell.lane) <= .12)
            continue;
        const auto localInverse = instance.transform.inverted(&valid);
        if (!valid)
            continue;
        const auto localStart = localInverse.map(start), delta = localInverse.map(end) - localStart;
        double enter = 0, exit = 1;
        const std::array<double, 3> lo{ -2.225, .175, -.07 }, hi{ 2.225, 3.525, .07 };
        for (int axis = 0; axis < 3; ++axis) {
            if (std::abs(delta[axis]) < 1e-10) {
                if (localStart[axis] < lo[axis] || localStart[axis] > hi[axis])
                    exit = -1;
            } else {
                double a = (lo[axis] - localStart[axis]) / delta[axis];
                double b = (hi[axis] - localStart[axis]) / delta[axis];
                if (a > b)
                    std::swap(a, b);
                enter = std::max(enter, a);
                exit = std::min(exit, b);
            }
        }
        if (enter > exit || enter >= nearest)
            continue;
        nearest = enter;
        const auto local = localStart + delta * float(enter);
        hit = { { "albumId", instance.albumId },
                { "objectId", QVariant::fromValue(instance.objectId) },
                { "lane", instance.cell.lane },
                { "row", instance.cell.row },
                { "localPoint", QVariant::fromValue(local) },
                { "worldPoint", QVariant::fromValue(instance.transform.map(local)) },
                { "frameRevision", QVariant::fromValue(frame.revision) } };
    }
    return hit;
}
}
