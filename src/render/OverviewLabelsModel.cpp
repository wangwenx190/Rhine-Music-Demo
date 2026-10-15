#include "OverviewLabelsModel.h"
#include <QPointF>
#include <algorithm>
#include <cmath>

namespace yalahoo {
namespace {
QEasingCurve curve(double x1, double y1, double x2, double y2)
{
    QEasingCurve result(QEasingCurve::BezierSpline);
    result.addCubicBezierSegment({ x1, y1 }, { x2, y2 }, { 1, 1 });
    return result;
}
const auto entryCurve = curve(.4, 0, .2, 1);
const auto opacityCurve = curve(.25, .1, .25, 1);
}
OverviewLabelsModel::OverviewLabelsModel(QObject *parent) : QAbstractListModel(parent)
{
    for (auto &slot : m_slots) {
        slot = std::make_unique<Slot>();
        slot->fade.setEnterCurve(entryCurve);
    }
}
int OverviewLabelsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : capacity;
}
QHash<int, QByteArray> OverviewLabelsModel::roleNames() const
{
    return { { Lane, "lane" },
             { Name, "columnName" },
             { Count, "albumCount" },
             { X, "anchorX" },
             { Y, "anchorY" },
             { Extent, "extent" },
             { Selected, "selectedColumn" },
             { Interactive, "interactive" },
             { Expanded, "expanded" },
             { Fade, "fade" },
             { Offset, "offset" },
             { EntryWidth, "entryWidth" },
             { EntryOpacity, "entryOpacity" },
             { EntryOffset, "entryOffset" } };
}
QVariant OverviewLabelsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= capacity)
        return { };
    const auto &s = *m_slots[index.row()];
    const auto &c = s.column;
    switch (role) {
    case Lane:
        return c.lane;
    case Name:
        return c.name;
    case Count:
        return c.count;
    case X:
        return c.x;
    case Y:
        return c.y;
    case Extent:
        return c.extent;
    case Selected:
        return c.selected;
    case Interactive:
        return s.wanted;
    case Expanded:
        return c.lane == m_expanded && s.wanted;
    case Fade:
        return s.fade.opacity();
    case Offset:
        return s.fade.offset();
    case EntryWidth:
        return 76 * s.entry;
    case EntryOpacity:
        return s.entryOpacity;
    case EntryOffset:
        return -6 * (1 - s.entry);
    default:
        return { };
    }
}
QVector<OverviewColumn> OverviewLabelsModel::visibleLabels(QVector<OverviewColumn> columns,
                                                           double width, double height,
                                                           const QSet<int> &retained, int expanded)
{
    // The inclusive physical interval has ten candidates at a half-lane boundary.
    // Cull its zero-extent endpoints after priority sorting; never truncate a selected ID.
    columns.resize(std::min(columns.size(), qsizetype(10)));
    struct Candidate
    {
        OverviewColumn column;
        bool expanded;
        double centerDistance;
    };
    QVector<Candidate> candidates;
    candidates.reserve(columns.size());
    for (const auto &column : columns)
        candidates.append({ column, column.lane == expanded, std::abs(column.x - width / 2) });
    std::stable_sort(candidates.begin(), candidates.end(), [](const auto &a, const auto &b) {
        if (a.expanded != b.expanded)
            return a.expanded;
        if (a.column.selected != b.column.selected)
            return a.column.selected;
        return a.centerDistance < b.centerDistance;
    });
    const double labelWidth = width < 600 ? 116 : 160;
    QVector<OverviewColumn> accepted;
    for (const auto &candidate : candidates) {
        const auto &c = candidate.column;
        const bool keep = retained.contains(c.lane);
        if (!std::isfinite(c.x) || !std::isfinite(c.y) || !std::isfinite(c.extent)
            || c.extent <= (keep ? .06 : .12))
            continue;
        if (c.x < labelWidth / 2 + (keep ? -18 : 14)
            || c.x > width - labelWidth / 2 - 76 - (keep ? 14 : 34) || c.y < 115 - (keep ? 20 : 0)
            || c.y > height - 112 + (keep ? 20 : 0))
            continue;
        const double left = c.x - labelWidth / 2;
        const double right = c.x + labelWidth / 2 + (c.lane == expanded ? 76 : 0);
        const bool collision = std::any_of(accepted.cbegin(), accepted.cend(), [&](const auto &o) {
            return left < o.x + labelWidth / 2 + (o.lane == expanded ? 76 : 0) + 10
                    && right + 10 > o.x - labelWidth / 2 && std::abs(o.y - c.y) < 62;
        });
        if (!collision && accepted.size() < 9)
            accepted.append(c);
    }
    std::sort(accepted.begin(), accepted.end(), [](const auto &a, const auto &b) {
        return a.x < b.x;
    });
    return accepted;
}
void OverviewLabelsModel::update(const QVector<OverviewColumn> &columns, double width,
                                 double height, bool ready, double dt, bool reduced)
{
    std::array<bool, capacity> previouslyLive;
    for (int row = 0; row < capacity; ++row)
        previouslyLive[row] = m_slots[row]->column.lane >= 0;
    QSet<int> retained;
    for (const auto &s : m_slots)
        if (s->wanted)
            retained.insert(s->column.lane);
    const auto visible = ready ? visibleLabels(columns, width, height, retained, m_expanded)
                               : QVector<OverviewColumn>{ };
    QSet<int> wanted;
    for (const auto &c : visible)
        wanted.insert(c.lane);
    for (auto &s : m_slots) {
        auto found = std::find_if(columns.cbegin(), columns.cend(), [&](const auto &c) {
            return c.lane == s->column.lane;
        });
        if (found != columns.cend())
            s->column = *found;
        else if (s->column.lane >= 0)
            s->column.extent = 0; // Never leave an old anchor visible across physical recycling.
        if (s->wanted && !wanted.contains(s->column.lane)) {
            s->wanted = false;
            s->fade.hide(reduced);
            if (m_expanded == s->column.lane)
                m_expanded = -1;
        }
        s->fade.advance(dt);
        if (reduced)
            s->fade.finish();
        if (!s->wanted && !s->fade.shown())
            s->column.lane = -1;
    }
    for (const auto &c : visible) {
        auto it = std::find_if(m_slots.begin(), m_slots.end(), [&](const auto &s) {
            return s->column.lane == c.lane;
        });
        if (it == m_slots.end())
            it = std::find_if(m_slots.begin(), m_slots.end(), [](const auto &s) {
                return s->column.lane < 0;
            });
        if (it == m_slots.end())
            continue; // Fixed retiring capacity; never evict a still-visible card.
        auto &s = **it;
        s.column = c;
        if (!s.wanted) {
            s.wanted = true;
            s.fade.show(reduced);
        }
    }
    for (int row = 0; row < capacity; ++row) {
        auto &s = *m_slots[row];
        const double target = s.wanted && s.column.lane == m_expanded ? 1 : 0;
        if (target != s.entryTarget) {
            s.entryFrom = s.entry;
            s.opacityFrom = s.entryOpacity;
            s.entryTime = s.opacityTime = 0;
            s.entryTarget = target;
        }
        s.entryTime = std::min(.42, s.entryTime + dt);
        s.opacityTime = std::min(.32, s.opacityTime + dt);
        s.entry = reduced ? target
                          : s.entryFrom
                        + (target - s.entryFrom) * entryCurve.valueForProgress(s.entryTime / .42);
        s.entryOpacity = reduced ? target
                                 : s.opacityFrom
                        + (target - s.opacityFrom)
                                * opacityCurve.valueForProgress(s.opacityTime / .32);
        if (previouslyLive[row] || s.column.lane >= 0 || s.fade.running())
            emit dataChanged(index(row), index(row));
    }
}
bool OverviewLabelsModel::expand(int lane)
{
    for (const auto &s : m_slots)
        if (s->wanted && s->column.lane == lane) {
            m_expanded = lane;
            return true;
        }
    return false;
}
void OverviewLabelsModel::collapse()
{
    m_expanded = -1;
}
bool OverviewLabelsModel::canEnter(int lane) const
{
    return lane >= 0 && lane == m_expanded
            && std::any_of(m_slots.cbegin(), m_slots.cend(), [&](const auto &s) {
                   return s->wanted && s->column.lane == lane;
               });
}
int OverviewLabelsModel::expandedLane() const
{
    return m_expanded;
}
void OverviewLabelsModel::clear()
{
    m_expanded = -1;
    for (auto &s : m_slots) {
        s->wanted = false;
        s->fade.reset(false);
        s->column = { };
        s->entry = s->entryOpacity = s->entryTarget = s->entryFrom = s->opacityFrom = 0;
    }
    emit dataChanged(index(0), index(capacity - 1));
}
int OverviewLabelsModel::liveCount() const
{
    return int(std::count_if(m_slots.cbegin(), m_slots.cend(), [](const auto &s) {
        return s->column.lane >= 0;
    }));
}
bool OverviewLabelsModel::running() const
{
    return std::any_of(m_slots.cbegin(), m_slots.cend(), [](const auto &s) {
        return s->fade.running() || s->entryTime < .42 || s->opacityTime < .32;
    });
}

}
