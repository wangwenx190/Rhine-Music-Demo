#include "EasingCurve.h"
#include "OverviewLabelsModel.h"
#include <QPointF>
#include <algorithm>
#include <iterator>
#include <cmath>

using namespace Qt::StringLiterals;

namespace yalahoo {
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
    static const QHash<int, QByteArray> roles = { { Lane, "lane"_ba },
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
    return roles;
}
QVariant OverviewLabelsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.model() != this || index.column() != 0 || index.row() < 0
        || index.row() >= capacity)
        return { };
    const auto &s = *m_slots[index.row()];
    struct RoleReader final
    {
        int role;
        QVariant (*read)(const Slot &s, int expanded);
    };
    static constexpr RoleReader readers[] = { { Lane,
                                                [](const Slot &s, int) -> QVariant {
                                                    return s.column.lane;
                                                } },
                                              { Name,
                                                [](const Slot &s, int) -> QVariant {
                                                    return s.column.name;
                                                } },
                                              { Count,
                                                [](const Slot &s, int) -> QVariant {
                                                    return s.column.count;
                                                } },
                                              { X,
                                                [](const Slot &s, int) -> QVariant {
                                                    return s.column.x;
                                                } },
                                              { Y,
                                                [](const Slot &s, int) -> QVariant {
                                                    return s.column.y;
                                                } },
                                              { Extent,
                                                [](const Slot &s, int) -> QVariant {
                                                    return s.column.extent;
                                                } },
                                              { Selected,
                                                [](const Slot &s, int) -> QVariant {
                                                    return s.column.selected;
                                                } },
                                              { Interactive,
                                                [](const Slot &s, int) -> QVariant {
                                                    return s.wanted;
                                                } },
                                              { Expanded,
                                                [](const Slot &s, int expanded) -> QVariant {
                                                    return s.column.lane == expanded && s.wanted;
                                                } },
                                              { Fade,
                                                [](const Slot &s, int) -> QVariant {
                                                    return s.fade.opacity();
                                                } },
                                              { Offset,
                                                [](const Slot &s, int) -> QVariant {
                                                    return s.fade.offset();
                                                } },
                                              { EntryWidth,
                                                [](const Slot &s, int) -> QVariant {
                                                    return EntryButtonWidthPixels * s.entry;
                                                } },
                                              { EntryOpacity,
                                                [](const Slot &s, int) -> QVariant {
                                                    return s.entryOpacity;
                                                } },
                                              { EntryOffset, [](const Slot &s, int) -> QVariant {
                                                   return -EntryHiddenOffsetPixels * (1 - s.entry);
                                               } } };
    static_assert([] {
        int next = Lane;
        for (const auto &reader : readers) {
            if (reader.role != next++)
                return false;
        }
        return next == EntryOffset + 1;
    }());
    if (role < Lane || role > EntryOffset)
        return { };
    return readers[role - Lane].read(s, m_expanded);
}
// Prioritize expanded/selected labels before collision admission. Retained anchors use looser
// visibility bounds so small projection changes do not repeatedly destroy and recreate a card.
QVector<OverviewColumn> OverviewLabelsModel::visibleLabels(const QVector<OverviewColumn> &columns,
                                                           double width, double height,
                                                           std::span<const int> retained,
                                                           int expanded)
{
    // The inclusive physical interval has ten candidates at a half-lane boundary.
    // Cull its zero-extent endpoints after priority sorting; never truncate a selected ID.
    qsizetype count = 0;
    struct Candidate final
    {
        qsizetype index;
        bool expanded;
        double centerDistance;
    };
    std::array<Candidate, CandidateCountWithBoundary> candidates;
    const auto limit = std::min(columns.size(), qsizetype(CandidateCountWithBoundary));
    for (qsizetype i = 0; i < limit; ++i) {
        const auto &column = columns[i];
        if (!std::isfinite(column.x) || !std::isfinite(column.y) || !std::isfinite(column.extent))
            continue;
        candidates[count++] = { i, column.lane == expanded, std::abs(column.x - width / 2) };
    }
    const auto end = candidates.begin() + count;
    std::sort(candidates.begin(), end, [&columns](const auto &a, const auto &b) {
        if (a.expanded != b.expanded)
            return a.expanded;
        if (columns[a.index].selected != columns[b.index].selected)
            return columns[a.index].selected;
        if (a.centerDistance != b.centerDistance)
            return a.centerDistance < b.centerDistance;
        return a.index < b.index;
    });
    const double labelWidth =
            width < NarrowViewportPixels ? NarrowLabelWidthPixels : LabelWidthPixels;
    QVector<OverviewColumn> accepted;
    accepted.reserve(std::min(count, qsizetype(MaximumVisibleLabels)));
    for (auto it = candidates.begin(); it != end; ++it) {
        const auto &c = columns[it->index];
        const bool keep = std::find(retained.begin(), retained.end(), c.lane) != retained.end();
        if (c.extent <= (keep ? RetainedExtentThreshold : NewExtentThreshold))
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
    std::array<int, capacity> retained;
    qsizetype retainedCount = 0;
    for (const auto &s : m_slots)
        if (s->wanted)
            retained[retainedCount++] = s->column.lane;
    const auto visible = ready
            ? visibleLabels(columns, width, height, std::span(retained.data(), retainedCount),
                            m_expanded)
            : QVector<OverviewColumn>{ };
    for (auto &s : m_slots) {
        auto found = std::find_if(columns.cbegin(), columns.cend(), [&](const auto &c) {
            return c.lane == s->column.lane;
        });
        if (found != columns.cend())
            s->column = *found;
        else if (s->column.lane >= 0)
            s->column.extent = 0; // Never leave an old anchor visible across physical recycling.
        if (s->wanted && std::none_of(visible.cbegin(), visible.cend(), [&](const auto &column) {
                return column.lane == s->column.lane;
            })) {
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
        notifySlot(row);
    }
}
void OverviewLabelsModel::notifySlot(int row)
{
    auto &slot = *m_slots[row];
    const PublishedSlot next{
        slot.column,         slot.wanted,        slot.column.lane == m_expanded && slot.wanted,
        slot.fade.opacity(), slot.fade.offset(), slot.entry,
        slot.entryOpacity
    };
    const auto &before = slot.published;
    QList<int> roles;
    const auto changed = [&roles](const auto &oldValue, const auto &newValue, int role) {
        if (oldValue != newValue)
            roles.append(role);
    };
    changed(before.column.lane, next.column.lane, Lane);
    changed(before.column.name, next.column.name, Name);
    changed(before.column.count, next.column.count, Count);
    changed(before.column.x, next.column.x, X);
    changed(before.column.y, next.column.y, Y);
    changed(before.column.extent, next.column.extent, Extent);
    changed(before.column.selected, next.column.selected, Selected);
    changed(before.wanted, next.wanted, Interactive);
    changed(before.expanded, next.expanded, Expanded);
    changed(before.fade, next.fade, Fade);
    changed(before.offset, next.offset, Offset);
    changed(before.entry, next.entry, EntryWidth);
    changed(before.entry, next.entry, EntryOffset);
    changed(before.entryOpacity, next.entryOpacity, EntryOpacity);
    // Commit before notifying: synchronous readers may expand, clear or replace labels.
    slot.published = next;
    if (!roles.isEmpty())
        Q_EMIT dataChanged(index(row), index(row), roles);
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
    for (int row = 0; row < capacity; ++row)
        notifySlot(row);
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
