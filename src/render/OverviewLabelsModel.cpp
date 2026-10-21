#include "EasingCurve.h"
#include "OverviewLabelsModel.h"
#include <QPointF>
#include <algorithm>
#include <cmath>

namespace {
// Admission hysteresis keeps retiring labels stable while collision checks reserve entry-button width.
constexpr auto EntryBezierFirstX = .4;
constexpr auto EntryBezierSecondX = .2;
constexpr auto OpacityBezierControlX = .25;
constexpr auto OpacityBezierFirstY = .1;
constexpr auto EntryButtonWidthPixels = 76;
constexpr auto EntryHiddenOffsetPixels = 6;
constexpr auto CandidateCountWithBoundary = 10;
constexpr auto HorizontalCollisionGapPixels = 10;
constexpr auto NarrowViewportPixels = 600;
constexpr auto NarrowLabelWidthPixels = 116;
constexpr auto LabelWidthPixels = 160;
constexpr auto RetainedExtentThreshold = .06;
constexpr auto NewExtentThreshold = .12;
constexpr auto RetainedLeftSlackPixels = 18;
constexpr auto AdmissionInsetPixels = 14;
constexpr auto NewRightInsetPixels = 34;
constexpr auto TopInsetPixels = 115;
constexpr auto RetainedVerticalSlackPixels = 20;
constexpr auto BottomInsetPixels = 112;
constexpr auto VerticalCollisionPixels = 62;
constexpr auto MaximumVisibleLabels = 9;
constexpr auto EntrySlideSeconds = .42;
constexpr auto EntryFadeSeconds = .32;
}

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
// Qt's spline storage is initialized lazily and reused without per-frame allocation.
const QEasingCurve &entryCurve()
{
    static const auto value = cubicBezier(EntryBezierFirstX, 0, EntryBezierSecondX, 1);
    return value;
}
// Qt's spline storage is initialized lazily and reused without per-frame allocation.
const QEasingCurve &opacityCurve()
{
    static const auto value =
            cubicBezier(OpacityBezierControlX, OpacityBezierFirstY, OpacityBezierControlX, 1);
    return value;
}
}
OverviewLabelsModel::OverviewLabelsModel(QObject *parent) : QAbstractListModel(parent)
{
    for (auto &slot : m_slots) {
        slot = std::make_unique<Slot>();
        slot->fade.setEnterCurve(entryCurve());
    }
}
int OverviewLabelsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : capacity;
}
QHash<int, QByteArray> OverviewLabelsModel::roleNames() const
{
    return { { Lane, "lane"_ba },
             { Name, "columnName"_ba },
             { Count, "albumCount"_ba },
             { X, "anchorX"_ba },
             { Y, "anchorY"_ba },
             { Extent, "extent"_ba },
             { Selected, "selectedColumn"_ba },
             { Interactive, "interactive"_ba },
             { Expanded, "expanded"_ba },
             { Fade, "fade"_ba },
             { Offset, "offset"_ba },
             { EntryWidth, "entryWidth"_ba },
             { EntryOpacity, "entryOpacity"_ba },
             { EntryOffset, "entryOffset"_ba } };
}
QVariant OverviewLabelsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.model() != this || index.column() != 0 || index.row() < 0
        || index.row() >= capacity)
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
        return EntryButtonWidthPixels * s.entry;
    case EntryOpacity:
        return s.entryOpacity;
    case EntryOffset:
        return -EntryHiddenOffsetPixels * (1 - s.entry);
    default:
        return { };
    }
}
// Prioritize expanded/selected labels before collision admission. Retained anchors use looser
// visibility bounds so small projection changes do not repeatedly destroy and recreate a card.
QVector<OverviewColumn> OverviewLabelsModel::visibleLabels(QVector<OverviewColumn> columns,
                                                           double width, double height,
                                                           const QSet<int> &retained, int expanded)
{
    // The inclusive physical interval has ten candidates at a half-lane boundary.
    // Cull its zero-extent endpoints after priority sorting; never truncate a selected ID.
    columns.resize(std::min(columns.size(), qsizetype(CandidateCountWithBoundary)));
    struct Candidate final
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
    const double labelWidth =
            width < NarrowViewportPixels ? NarrowLabelWidthPixels : LabelWidthPixels;
    QVector<OverviewColumn> accepted;
    for (const auto &candidate : candidates) {
        const auto &c = candidate.column;
        const bool keep = retained.contains(c.lane);
        if (!std::isfinite(c.x) || !std::isfinite(c.y) || !std::isfinite(c.extent)
            || c.extent <= (keep ? RetainedExtentThreshold : NewExtentThreshold))
            continue;
        if (c.x < labelWidth / 2 + (keep ? -RetainedLeftSlackPixels : AdmissionInsetPixels)
            || c.x > width - labelWidth / 2 - EntryButtonWidthPixels
                            - (keep ? AdmissionInsetPixels : NewRightInsetPixels)
            || c.y < TopInsetPixels - (keep ? RetainedVerticalSlackPixels : 0)
            || c.y > height - BottomInsetPixels + (keep ? RetainedVerticalSlackPixels : 0))
            continue;
        const double left = c.x - labelWidth / 2;
        const double right =
                c.x + labelWidth / 2 + (c.lane == expanded ? EntryButtonWidthPixels : 0);
        const bool collision = std::any_of(accepted.cbegin(), accepted.cend(), [&](const auto &o) {
            return left < o.x + labelWidth / 2 + (o.lane == expanded ? EntryButtonWidthPixels : 0)
                            + HorizontalCollisionGapPixels
                    && right + HorizontalCollisionGapPixels > o.x - labelWidth / 2
                    && std::abs(o.y - c.y) < VerticalCollisionPixels;
        });
        if (!collision && accepted.size() < MaximumVisibleLabels)
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
        if (!s->wanted && !s->fade.shown()) {
            s->column.lane = -1;
            // The 240ms card exit can finish before its 420ms entry collapse.
            // A recycled physical slot must not inherit another column's entry.
            s->entry = s->entryOpacity = s->entryTarget = s->entryFrom = s->opacityFrom = 0;
            s->entryTime = s->opacityTime = 0;
        }
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
        s.entryTime = std::min(EntrySlideSeconds, s.entryTime + dt);
        s.opacityTime = std::min(EntryFadeSeconds, s.opacityTime + dt);
        s.entry = reduced ? target
                          : s.entryFrom
                        + (target - s.entryFrom)
                                * entryCurve().valueForProgress(s.entryTime / EntrySlideSeconds);
        s.entryOpacity = reduced ? target
                                 : s.opacityFrom
                        + (target - s.opacityFrom)
                                * opacityCurve().valueForProgress(s.opacityTime / EntryFadeSeconds);
        if (previouslyLive[row] || s.column.lane >= 0 || s.fade.running())
            Q_EMIT dataChanged(index(row), index(row));
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
    Q_EMIT dataChanged(index(0), index(capacity - 1));
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
        return s->fade.running() || s->entryTime < EntrySlideSeconds
                || s->opacityTime < EntryFadeSeconds;
    });
}

}
