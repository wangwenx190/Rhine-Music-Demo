#include "RealColumns.h"
#include <algorithm>
#include <cmath>

namespace {
// Finite column bounds are expressed in source row coordinates; detail and browse use different rates.
constexpr auto FirstAlbumRow = 12;
constexpr auto FirstAlbumRowPosition = 12.0;
constexpr auto LastRowOffset = 11.;
constexpr auto DetailFollowRate = 9;
constexpr auto BrowseFollowRate = 3.7;
}

namespace yalahoo {
void RealColumns::reset(const QList<int> &counts, ArchiveCell selected,
                        std::optional<double> displayedRow)
{
    m_columns.clear();
    m_time = 0;
    m_reducedEpoch = 0;
    if (!std::isfinite(selected.lane) || std::floor(selected.lane) != selected.lane
        || selected.lane < 0 || selected.lane >= counts.size() || counts[int(selected.lane)] <= 0) {
        selected.lane = -1;
        for (int lane = 0; lane < counts.size(); ++lane)
            if (counts[lane] > 0) {
                selected.lane = lane;
                break;
            }
    }
    if (selected.lane >= 0) {
        const double midpoint = FirstAlbumRow + (counts[int(selected.lane)] - 1) / 2.0;
        selected.row =
                std::clamp(std::isfinite(selected.row) ? selected.row : midpoint,
                           FirstAlbumRowPosition, LastRowOffset + counts[int(selected.lane)]);
    } else
        selected.row = FirstAlbumRow;
    m_selected = selected;
    double distance = 0;
    for (int lane = 0; lane < counts.size(); ++lane) {
        const int count = std::max(0, counts[lane]);
        const double midpoint = FirstAlbumRow + std::max(0, count - 1) / 2.0;
        const double target = lane == selected.lane && count > 0
                ? std::clamp(selected.row, FirstAlbumRowPosition, LastRowOffset + count)
                : midpoint;
        const double initial =
                displayedRow && std::isfinite(*displayedRow) ? *displayedRow : target;
        m_columns.append({ count, midpoint, ColumnScalar(initial),
                           ColumnScalar(double(lane == selected.lane)), 0, 0 });
        distance = std::max(distance, std::abs(target - initial));
    }
    const double duration = archiveTravelDuration(0, distance);
    for (int lane = 0; lane < m_columns.size(); ++lane) {
        auto &column = m_columns[lane];
        column.row.retarget(lane == selected.lane ? selected.row : column.midpoint, true, duration);
    }
}
RealColumns::Column *RealColumns::sample(int lane) const
{
    if (lane < 0 || lane >= m_columns.size())
        return nullptr;
    auto &column = m_columns[lane];
    if (column.reducedEpoch != m_reducedEpoch) {
        column.row.finish();
        column.activity.finish();
        column.reducedEpoch = m_reducedEpoch;
    }
    const double dt = m_time - column.sampledAt;
    if (dt > 0) {
        column.row.update(dt);
        column.activity.update(dt);
    }
    column.sampledAt = m_time;
    return &column;
}
void RealColumns::select(ArchiveCell cell, bool guided, bool detail)
{
    if (!std::isfinite(cell.lane) || !std::isfinite(cell.row) || std::floor(cell.lane) != cell.lane
        || cell.lane < 0 || cell.lane >= m_columns.size())
        return;
    auto *current = sample(int(cell.lane));
    if (!current || current->count == 0)
        return;
    cell.row = std::clamp(cell.row, FirstAlbumRowPosition, LastRowOffset + current->count);
    if (cell == m_selected)
        return;
    const bool changed = cell.lane != m_selected.lane;
    auto *old = sample(int(m_selected.lane));
    const bool continueGuided = !detail && (guided || changed || current->row.guided());
    const double distance =
            std::max(std::abs(cell.row - current->row.state().value),
                     changed && old ? std::abs(old->midpoint - old->row.state().value) : 0.0);
    const double duration =
            archiveTravelDuration(changed ? cell.lane - m_selected.lane : 0, distance);
    const double rate = detail ? DetailFollowRate : BrowseFollowRate;
    if (changed && old) {
        old->row.retarget(old->midpoint, continueGuided, duration, rate);
        old->activity.retarget(0, continueGuided, duration, rate);
    }
    current->row.retarget(cell.row, continueGuided, duration, rate);
    current->activity.retarget(1, continueGuided, duration, rate);
    m_selected = cell;
}
void RealColumns::update(double dt, bool reduced)
{
    if (!std::isfinite(dt) || dt < 0)
        return;
    m_time += dt;
    if (reduced)
        ++m_reducedEpoch;
}
double RealColumns::row(int lane, double overview) const
{
    const auto *column = sample(lane);
    return column ? column->row.state().value
                    + (column->midpoint - column->row.state().value)
                            * std::clamp(overview, 0.0, 1.0)
                  : FirstAlbumRow;
}
double RealColumns::activity(int lane) const
{
    const auto *column = sample(lane);
    return column ? std::clamp(column->activity.state().value, 0.0, 1.0) : 0;
}
Kinematics RealColumns::rowState(int lane) const
{
    const auto *column = sample(lane);
    return column ? column->row.state() : Kinematics{ FirstAlbumRow, 0, 0 };
}
Kinematics RealColumns::activityState(int lane) const
{
    const auto *column = sample(lane);
    return column ? column->activity.state() : Kinematics{ };
}
bool RealColumns::settled(int lane) const
{
    const auto *column = sample(lane);
    return !column || (column->row.settled() && column->activity.settled());
}
}
