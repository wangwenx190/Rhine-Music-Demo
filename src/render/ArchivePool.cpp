#include "ArchivePool.h"
#include <cmath>
#include <limits>
#include <numeric>
#include <QSet>

using namespace Qt::StringLiterals;

namespace yalahoo {
int ArchivePool::wrap(double value, int count)
{
    if (count <= 0 || !std::isfinite(value))
        return 0;
    const auto remainder = std::fmod(value, count);
    return remainder < 0 ? count + int(std::floor(remainder)) : int(remainder);
}
double ArchivePool::nearestOccurrence(double value, double center, double period)
{
    if (period <= 0)
        return value;
    return value + std::floor((center - value + period / 2) / period) * period;
}
ArchiveCell ArchivePool::poolCell(int index)
{
    constexpr std::array<int, lanes> order{ 0, 1, 2, 3, 4, -2, -1, 5, 6 };
    return { double(order.at(index / rows)), double(index % rows - (rows - 32) / 2) };
}
ArchiveCell ArchivePool::visibleCell(int index, ArchiveCell center)
{
    const auto cell = poolCell(index);
    return { nearestOccurrence(cell.lane, center.lane, lanes),
             nearestOccurrence(cell.row, center.row, rows) };
}
void ArchivePool::setColumns(QList<LibraryColumn> columns)
{
    m_columns = std::move(columns);
    QSet<QString> identities;
    for (int lane = 0; lane < m_columns.size(); ++lane) {
        auto &column = m_columns[lane];
        if (column.key.isEmpty())
            column.key = u"lane:%1"_s.arg(lane);
        column.albumIds.removeIf([&](const QString &id) {
            if (id.isEmpty() || identities.contains(id))
                return true;
            identities.insert(id);
            return false;
        });
    }
    m_locations.clear();
    m_rowPeriod = 1;
    for (int lane = 0; lane < m_columns.size(); ++lane) {
        const auto &ids = m_columns[lane].albumIds;
        for (int row = 0; row < ids.size(); ++row)
            m_locations.insert(ids[row], { double(lane), double(row + 12) });
        if (std::isfinite(m_rowPeriod)) {
            const auto count = qMax(1, int(ids.size()));
            const auto next =
                    qint64(m_rowPeriod) / std::gcd(qint64(m_rowPeriod), qint64(count)) * count;
            m_rowPeriod = next > 1000000 ? std::numeric_limits<double>::infinity() : double(next);
        }
    }
}
QString ArchivePool::albumAt(ArchiveCell cell) const
{
    if (m_columns.isEmpty())
        return { };
    const auto &ids = m_columns[wrap(cell.lane, int(m_columns.size()))].albumIds;
    return ids.isEmpty() ? QString() : ids[wrap(cell.row - 12, int(ids.size()))];
}
std::optional<ArchiveCell> ArchivePool::location(const QString &id) const
{
    const auto it = m_locations.constFind(id);
    return it == m_locations.cend() ? std::nullopt : std::optional<ArchiveCell>(*it);
}
int ArchivePool::columnCount() const
{
    return int(m_columns.size());
}
int ArchivePool::rowCount(double lane) const
{
    return m_columns.isEmpty() ? 0
                               : int(m_columns[wrap(lane, int(m_columns.size()))].albumIds.size());
}
double ArchivePool::rowPeriod() const
{
    return m_rowPeriod;
}
const QList<LibraryColumn> &ArchivePool::columns() const
{
    return m_columns;
}
QString ArchivePool::realAlbumAt(ArchiveCell cell) const
{
    if (!std::isfinite(cell.lane) || !std::isfinite(cell.row) || cell.lane < 0
        || cell.lane >= m_columns.size() || std::floor(cell.lane) != cell.lane
        || std::floor(cell.row) != cell.row)
        return { };
    const auto &ids = m_columns[int(cell.lane)].albumIds;
    return cell.row >= 12 && cell.row < 12 + ids.size() ? ids[int(cell.row - 12)] : QString();
}
void ArchivePool::update(ArchiveCell center, const std::array<double, lanes> *columnRows)
{
    for (int i = 0; i < capacity; ++i) {
        auto &instance = m_instances[i];
        instance.cell = visibleCell(i, center);
        if (columnRows)
            instance.cell.row = nearestOccurrence(poolCell(i).row, (*columnRows)[i / rows], rows);
        instance.albumId = columnRows ? realAlbumAt(instance.cell) : albumAt(instance.cell);
        instance.objectId = quint64(i + 1);
        instance.owned = false;
    }
}
const std::array<ArchiveInstance, ArchivePool::capacity> &ArchivePool::instances() const
{
    return m_instances;
}
}
