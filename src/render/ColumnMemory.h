#pragma once
#include "ArchivePool.h"

namespace yalahoo {
// Session-only stable album IDs. Only the enabled preference is persisted.
class ColumnMemory
{
public:
    void reset(const QList<LibraryColumn> &columns);
    void remember(const QString &key, const QString &albumId);
    QString recalled(const QString &key) const;
    bool enabled = true;

private:
    QHash<QString, QString> m_albums;
};
struct NavigationAvailability
{
    bool previous = false, next = false, left = false, right = false;
    static NavigationAvailability at(int count, int ordinal, int columns, bool realistic);
    bool allowsRow(int direction) const;
};
}
