#pragma once
#include <QtClassHelperMacros>
#include <QHash>
#include <QList>
#include <QString>

namespace yalahoo {
struct LibraryColumn;
// Session-only stable album IDs. Only the enabled preference is persisted.
class ColumnMemory final
{
    Q_DISABLE_COPY_MOVE(ColumnMemory)
public:
    ColumnMemory() = default;
    void reset(const QList<LibraryColumn> &columns);
    void remember(const QString &key, const QString &albumId);
    QString recalled(const QString &key) const;
    bool enabled = true;

private:
    QHash<QString, QString> m_albums;
};
struct NavigationAvailability final
{
    bool previous = false, next = false, left = false, right = false;
    static NavigationAvailability at(int count, int ordinal, int columns, bool realistic);
    bool allowsRow(int direction) const;
};
}
