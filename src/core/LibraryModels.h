#pragma once
#include "MusicTypes.h"
#include <QAbstractListModel>
#include <QHash>
#include <QVariantList>

namespace yalahoo {
enum class SortMode {
    Genre,
    Artist,
    Album
};
struct LibraryColumn final
{
    QString key, label;
    QStringList albumIds;
};
struct LibraryOrder final
{
    QList<int> indices;
    QList<LibraryColumn> columns;
    int slotStride = 32;
};
QString normalizedKey(const QString &value);
LibraryOrder orderLibrary(const LibrarySnapshot &snapshot, SortMode mode);
struct SearchResult final
{
    QString albumId, trackId;
    bool operator==(const SearchResult &) const = default;
};
QString genreDisplayName(const Genre &genre);
QString artistDisplayName(const QString &artist);
QList<SearchResult> searchLibrary(const LibrarySnapshot &snapshot, const QString &query,
                                  const QString &genreId = { });

class AlbumListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AlbumListModel)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)
    Q_PROPERTY(QVariantList columns READ columns NOTIFY columnsChanged FINAL)
    Q_PROPERTY(int slotStride READ slotStride NOTIFY columnsChanged FINAL)
public:
    enum Role {
        AlbumId = Qt::UserRole + 1,
        Title,
        Artist,
        Year,
        GenreId,
        CoverUrl,
        Folder,
        Tracks,
        Producers,
        Description,
        DescriptionSource,
        Introduction,
        CreditsLookup,
        Online,
        LocalNote,
        Offline,
        AlbumData
    };
    Q_ENUM(Role)
    explicit AlbumListModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = { }) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setSnapshot(Snapshot snapshot);
    void updateMetadataSnapshot(Snapshot snapshot);
    void setSortMode(SortMode mode);
    QVariantList columns() const;
    int slotStride() const
    {
        return m_order.slotStride;
    }
    Snapshot snapshot() const
    {
        return m_snapshot;
    }
    const LibraryOrder &order() const
    {
        return m_order;
    }
Q_SIGNALS:
    void countChanged();
    void columnsChanged();

private:
    Snapshot m_snapshot;
    SortMode m_mode = SortMode::Genre;
    LibraryOrder m_order;
};
class SearchListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SearchListModel)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)
public:
    enum Role {
        Kind = Qt::UserRole + 1,
        AlbumId,
        TrackId,
        Title,
        Artist,
        CoverUrl,
        Subtitle
    };
    Q_ENUM(Role)
    explicit SearchListModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = { }) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QVariantMap resultAt(int row) const;
    void update(Snapshot snapshot, const QString &query, const QString &genreId);
    void updateMetadataSnapshot(Snapshot snapshot)
    {
        m_snapshot = std::move(snapshot);
    }
Q_SIGNALS:
    void countChanged();

private:
    struct SearchEntry final
    {
        SearchResult result;
        QString genreId, text;
        bool album = false;
    };
    Snapshot m_snapshot;
    QList<SearchEntry> m_entries;
    QHash<QString, QString> m_genreNames;
    QList<SearchResult> m_results;
    QHash<QString, qsizetype> m_albumIndices;
    QHash<QString, QHash<QString, qsizetype>> m_trackIndices;
};
}
