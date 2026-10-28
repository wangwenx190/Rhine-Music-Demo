#include "LibraryIndex.h"
#include <utility>

using namespace Qt::StringLiterals;

namespace yalahoo {
bool sameLibraryStructure(const LibrarySnapshot &before, const LibrarySnapshot &after)
{
    if (before.albums.size() != after.albums.size() || before.genres != after.genres
        || before.roots != after.roots || before.scan != after.scan
        || before.onlineEnabled != after.onlineEnabled)
        return false;
    if (before.fields != after.fields) {
        auto beforeFields = before.fields;
        auto afterFields = after.fields;
        beforeFields.remove(u"metadataConfiguration"_s);
        afterFields.remove(u"metadataConfiguration"_s);
        if (beforeFields != afterFields)
            return false;
    }
    for (qsizetype i = 0; i < before.albums.size(); ++i) {
        const auto &left = before.albums[i];
        const auto &right = after.albums[i];
        if (left.id != right.id || left.title != right.title || left.artist != right.artist
            || left.genreId != right.genreId || left.year != right.year
            || left.discCount != right.discCount || left.rawGenres != right.rawGenres
            || left.folder != right.folder || left.coverUrl != right.coverUrl
            || left.folderLabel != right.folderLabel || left.offline != right.offline
            || left.localNote != right.localNote || left.tracks != right.tracks)
            return false;
        // Enrichment alone owns this extension. Unknown non-metadata fields stay immutable.
        if (left.fields == right.fields)
            continue;
        auto leftFields = left.fields;
        auto rightFields = right.fields;
        leftFields.remove(u"_onlineGenres"_s);
        rightFields.remove(u"_onlineGenres"_s);
        if (leftFields != rightFields)
            return false;
    }
    return true;
}
LibraryIndex::LibraryIndex() : m_snapshot(std::make_shared<const LibrarySnapshot>())
{
}
void LibraryIndex::replace(Snapshot snapshot)
{
    m_snapshot = snapshot ? std::move(snapshot) : std::make_shared<const LibrarySnapshot>();
    m_albumIndices.clear();
    m_trackIndices.clear();
    m_albumIndices.reserve(m_snapshot->albums.size());
    m_trackIndices.reserve(m_snapshot->albums.size());
    for (qsizetype i = 0; i < m_snapshot->albums.size(); ++i) {
        const auto &entry = m_snapshot->albums[i];
        if (!m_albumIndices.tryEmplace(entry.id, i).inserted)
            continue;
        QHash<QString, qsizetype> tracks;
        tracks.reserve(entry.tracks.size());
        for (qsizetype row = 0; row < entry.tracks.size(); ++row)
            tracks.tryEmplace(entry.tracks[row].id, row);
        m_trackIndices.insert(entry.id, std::move(tracks));
    }
}
bool LibraryIndex::replaceMetadata(Snapshot snapshot)
{
    if (!snapshot || !sameLibraryStructure(*m_snapshot, *snapshot))
        return false;
    m_snapshot = std::move(snapshot);
    return true;
}
void LibraryIndex::setMetadataConfiguration(const QJsonValue &configuration)
{
    auto updated = *m_snapshot;
    updated.fields.insert(u"metadataConfiguration"_s, configuration);
    // Configuration does not change IDs or lookup positions. Keep both indices intact.
    m_snapshot = std::make_shared<const LibrarySnapshot>(std::move(updated));
}
Snapshot LibraryIndex::snapshot() const
{
    return m_snapshot;
}
const Album *LibraryIndex::album(const QString &id) const
{
    const auto index = m_albumIndices.constFind(id);
    return index == m_albumIndices.cend() ? nullptr : &m_snapshot->albums[*index];
}
int LibraryIndex::trackIndex(const QString &albumId, const QString &trackId) const
{
    const auto tracks = m_trackIndices.constFind(albumId);
    return tracks == m_trackIndices.cend() ? -1 : int(tracks->value(trackId, -1));
}
const Track *LibraryIndex::track(const QString &albumId, const QString &trackId) const
{
    const auto *entry = album(albumId);
    const auto row = trackIndex(albumId, trackId);
    return entry && row >= 0 ? &entry->tracks[row] : nullptr;
}
}
