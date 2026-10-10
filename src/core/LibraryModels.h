#pragma once
#include "MusicTypes.h"
#include <QAbstractListModel>
#include <QVariantList>

namespace yalahoo {
enum class SortMode { Genre, Artist, Album };
struct LibraryColumn { QString key, label; QStringList albumIds; };
struct LibraryOrder { QList<int> indices; QList<LibraryColumn> columns; int slotStride = 32; };
QString normalizedKey(const QString &value);
LibraryOrder orderLibrary(const LibrarySnapshot &snapshot, SortMode mode);
struct SearchResult { QString albumId, trackId; };
QList<SearchResult> searchLibrary(const LibrarySnapshot &snapshot, const QString &query, const QString &genreId = {});

class AlbumListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QVariantList columns READ columns NOTIFY columnsChanged)
    Q_PROPERTY(int slotStride READ slotStride NOTIFY columnsChanged)
public:
    enum Role { AlbumId = Qt::UserRole + 1, Title, Artist, Year, GenreId, CoverUrl, Folder,
        Tracks, Producers, Description, DescriptionSource, Introduction, CreditsLookup, Online, LocalNote, Offline, AlbumData };
    Q_ENUM(Role)
    explicit AlbumListModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setSnapshot(Snapshot snapshot);
    void setSortMode(SortMode mode);
    QVariantList columns() const;
    int slotStride() const { return m_order.slotStride; }
    Snapshot snapshot() const { return m_snapshot; }
signals:
    void countChanged();
    void columnsChanged();
private:
    Snapshot m_snapshot;
    SortMode m_mode = SortMode::Genre;
    LibraryOrder m_order;
};
class SearchListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    enum Role { Kind = Qt::UserRole + 1, AlbumId, TrackId, Title, Artist };
    Q_ENUM(Role)
    explicit SearchListModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void update(Snapshot snapshot, const QString &query, const QString &genreId);
signals:
    void countChanged();
private:
    Snapshot m_snapshot;
    QList<SearchResult> m_results;
};
}
