#include "LibraryModels.h"
#include "LibraryIndex.h"
#include <QCoreApplication>
#include <utility>

using namespace Qt::StringLiterals;
namespace yalahoo {
AlbumListModel::AlbumListModel(QObject *parent)
    : QAbstractListModel(parent), m_snapshot(std::make_shared<const LibrarySnapshot>())
{
}
int AlbumListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_order.indices.size());
}
QHash<int, QByteArray> AlbumListModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = { { AlbumId, "albumId"_ba },
                                                  { Title, "title"_ba },
                                                  { Artist, "artist"_ba },
                                                  { Year, "year"_ba },
                                                  { GenreId, "genreId"_ba },
                                                  { CoverUrl, "coverUrl"_ba },
                                                  { Folder, "folder"_ba },
                                                  { Tracks, "tracks"_ba },
                                                  { Producers, "producers"_ba },
                                                  { Description, "description"_ba },
                                                  { DescriptionSource, "descriptionSource"_ba },
                                                  { Introduction, "introduction"_ba },
                                                  { CreditsLookup, "creditsLookup"_ba },
                                                  { Online, "online"_ba },
                                                  { LocalNote, "localNote"_ba },
                                                  { Offline, "offline"_ba },
                                                  { AlbumData, "albumData"_ba } };
    return roles;
}
QVariant AlbumListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.model() != this || index.column() != 0 || index.row() < 0
        || index.row() >= rowCount())
        return { };
    const auto &album = m_snapshot->albums[m_order.indices[index.row()]];
    struct RoleReader final
    {
        int role;
        QVariant (*read)(const Album &album);
    };
    static constexpr RoleReader readers[] = {
        { AlbumId,
          [](const Album &album) -> QVariant {
              return album.id;
          } },
        { Title,
          [](const Album &album) -> QVariant {
              return album.title;
          } },
        { Artist,
          [](const Album &album) -> QVariant {
              return artistDisplayName(album.artist);
          } },
        { Year,
          [](const Album &album) -> QVariant {
              return album.year ? QVariant(*album.year) : QVariant{ };
          } },
        { GenreId,
          [](const Album &album) -> QVariant {
              return album.genreId;
          } },
        { CoverUrl,
          [](const Album &album) -> QVariant {
              return album.coverUrl;
          } },
        { Folder,
          [](const Album &album) -> QVariant {
              return album.folder;
          } },
        { Tracks,
          [](const Album &album) -> QVariant {
              QVariantList tracks;
              tracks.reserve(album.tracks.size());
              for (const auto &track : album.tracks) {
                  auto map = toJson(track).toVariantMap();
                  map.insert(u"sourceUrl"_s, track.sourceUrl);
                  tracks.append(std::move(map));
              }
              return tracks;
          } },
        { Producers,
          [](const Album &album) -> QVariant {
              return producerData(album.producers).toVariantList();
          } },
        { Description,
          [](const Album &album) -> QVariant {
              return album.description;
          } },
        { DescriptionSource,
          [](const Album &album) -> QVariant {
              return album.descriptionSource.toVariantMap();
          } },
        { Introduction,
          [](const Album &album) -> QVariant {
              return album.introduction.toVariantMap();
          } },
        { CreditsLookup,
          [](const Album &album) -> QVariant {
              return album.creditsLookup.toVariantMap();
          } },
        { Online,
          [](const Album &album) -> QVariant {
              return album.online.toVariantMap();
          } },
        { LocalNote,
          [](const Album &album) -> QVariant {
              return album.localNote;
          } },
        { Offline,
          [](const Album &album) -> QVariant {
              return album.offline;
          } },
        { AlbumData,
          [](const Album &album) -> QVariant {
              return toJson(album).toVariantMap();
          } }
    };
    static_assert([] {
        int next = AlbumId;
        for (const auto &reader : readers) {
            if (reader.role != next++)
                return false;
        }
        return next == AlbumData + 1;
    }());
    if (role < AlbumId || role > AlbumData)
        return { };
    return readers[role - AlbumId].read(album);
}
QString AlbumListModel::columnKind() const
{
    switch (m_mode) {
    case SortMode::Artist:
        return tr("Artists");
    case SortMode::Album:
        return tr("Album titles");
    case SortMode::Genre:
        return tr("Genres");
    }
    Q_UNREACHABLE();
}
void AlbumListModel::setSnapshot(Snapshot snapshot)
{
    const int previousCount = rowCount();
    beginResetModel();
    m_snapshot = snapshot ? std::move(snapshot) : std::make_shared<const LibrarySnapshot>();
    m_order = orderLibrary(*m_snapshot, m_mode);
    endResetModel();
    if (previousCount != rowCount())
        Q_EMIT countChanged();
    Q_EMIT columnsChanged();
}
void AlbumListModel::updateMetadataSnapshot(Snapshot snapshot)
{
    if (!snapshot || !sameLibraryStructure(*m_snapshot, *snapshot))
        return;
    const auto previous = m_snapshot;
    m_snapshot = std::move(snapshot);
    const auto updated = m_snapshot;
    const auto indices = m_order.indices;
    const auto mode = m_mode;
    for (int row = 0; row < indices.size(); ++row) {
        const auto position = indices[row];
        const auto &before = previous->albums[position];
        const auto &after = updated->albums[position];
        if (before.introduction != after.introduction || before.creditsLookup != after.creditsLookup
            || before.online != after.online || before.description != after.description
            || before.descriptionSource != after.descriptionSource
            || before.producers != after.producers || before.fields != after.fields)
            Q_EMIT dataChanged(index(row), index(row),
                               { Producers, Description, DescriptionSource, Introduction,
                                 CreditsLookup, Online, AlbumData });
        // A sorting observer resets every row; old row positions cannot be notified afterward.
        if (m_snapshot != updated || m_mode != mode)
            return;
    }
}
void AlbumListModel::setSortMode(SortMode mode)
{
    if (m_mode == mode)
        return;
    beginResetModel();
    m_mode = mode;
    m_order = orderLibrary(*m_snapshot, mode);
    endResetModel();
    Q_EMIT columnsChanged();
}
QVariantList AlbumListModel::columns() const
{
    QVariantList result;
    result.reserve(m_order.columns.size());
    int lane = 0;
    for (const auto &column : m_order.columns)
        result.append(QVariantMap{ { u"key"_s, column.key },
                                   { u"label"_s, column.label },
                                   { u"albumIds"_s, column.albumIds },
                                   { u"lane"_s, lane++ } });
    return result;
}
}
