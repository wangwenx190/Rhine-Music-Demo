#include "ArchivePool.h"
#include "ColumnMemory.h"

namespace yalahoo {
void ColumnMemory::reset(const QList<LibraryColumn> &columns)
{
    m_albums.clear();
    m_albums.reserve(columns.size());
    for (const auto &column : columns)
        if (!column.albumIds.isEmpty())
            m_albums.insert(column.key, column.albumIds.first());
}
void ColumnMemory::remember(const QString &key, const QString &albumId)
{
    if (enabled && !albumId.isEmpty())
        m_albums.insert(key, albumId);
}
QString ColumnMemory::recalled(const QString &key) const
{
    return enabled ? m_albums.value(key) : QString();
}
NavigationAvailability NavigationAvailability::at(int count, int ordinal, int columns,
                                                  bool realistic)
{
    const bool valid = ordinal >= 0 && ordinal < count;
    return { valid && (!realistic || ordinal > 0), valid && (!realistic || ordinal < count - 1),
             valid && columns > 1, valid && columns > 1 };
}
bool NavigationAvailability::allowsRow(int direction) const
{
    return direction == -1 ? previous : direction == 1 && next;
}
}
