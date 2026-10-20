// Maps a bounded physical archive pool onto canonical library cells. Wrapping preserves stable
// identity while recycled instances limit scene work.

#pragma once
#include "LibraryModels.h"
#include <QMatrix4x4>
#include <array>

namespace yalahoo {
struct ArchiveCell final
{
    double lane = 2, row = 12;
    bool operator==(const ArchiveCell &) const = default;
};
struct ArchiveInstance final
{
    ArchiveCell cell;
    QString albumId;
    quint64 objectId = 0;
    QMatrix4x4 transform;
    bool owned = false;
    double clarity = 0;
    bool operator==(const ArchiveInstance &) const = default;
};
class ArchivePool final
{
    Q_DISABLE_COPY_MOVE(ArchivePool)
public:
    ArchivePool() = default;
    static constexpr int lanes = 9, rows = 48, capacity = lanes * rows;
    static constexpr double columnSpacing = 5.2, rowSpacing = .62;
    static int wrap(double value, int count);
    static double nearestOccurrence(double value, double center, double period);
    static ArchiveCell poolCell(int index);
    static ArchiveCell visibleCell(int index, ArchiveCell center);
    void setColumns(QList<LibraryColumn> columns);
    QString albumAt(ArchiveCell cell) const;
    std::optional<ArchiveCell> location(const QString &albumId) const;
    int columnCount() const;
    int rowCount(double lane) const;
    double rowPeriod() const;
    const QList<LibraryColumn> &columns() const;
    QString realAlbumAt(ArchiveCell cell) const;
    void update(ArchiveCell center, const std::array<double, lanes> *columnRows = nullptr);
    const std::array<ArchiveInstance, capacity> &instances() const;

private:
    QList<LibraryColumn> m_columns;
    QHash<QString, ArchiveCell> m_locations;
    std::array<ArchiveInstance, capacity> m_instances;
    double m_rowPeriod = 1;
};
}
