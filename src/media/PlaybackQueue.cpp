#include "PlaybackQueue.h"
#include <QDir>
#include <utility>

namespace yalahoo {
namespace {
bool playableSource(const QUrl &url)
{
    return url.isLocalFile() && QDir::isAbsolutePath(url.toLocalFile()) && !url.hasQuery()
            && !url.hasFragment() && !url.toLocalFile().contains(QChar::Null);
}
}
PlaybackQueue::Selection PlaybackQueue::select(const Snapshot &snapshot, const Album &album,
                                               const QString &trackId)
{
    const bool reuse = snapshot && m_snapshot.lock() == snapshot && m_albumId == album.id
            && !m_tracks.isEmpty();
    QList<Track> candidate;
    QHash<QString, int> indices;
    if (!reuse) {
        candidate.reserve(album.tracks.size());
        indices.reserve(album.tracks.size());
        for (const auto &track : album.tracks)
            if (indices.tryEmplace(track.id, int(candidate.size())).inserted)
                candidate.append(track);
    }
    const auto target = (reuse ? m_indices : indices).value(trackId, -1);
    if (target < 0)
        return Selection::MissingTrack;
    const auto &tracks = reuse ? std::as_const(m_tracks) : std::as_const(candidate);
    if (!playableSource(tracks[target].sourceUrl))
        return Selection::InvalidSource;
    if (!reuse) {
        m_tracks = std::move(candidate);
        m_indices = std::move(indices);
        m_snapshot = snapshot;
        m_albumId = album.id;
    }
    m_index = target;
    return Selection::Accepted;
}
bool PlaybackQueue::selectIndex(int index)
{
    if (index < 0 || index >= m_tracks.size()
        || !playableSource(std::as_const(m_tracks)[index].sourceUrl))
        return false;
    m_index = index;
    return true;
}
const Track *PlaybackQueue::current() const
{
    return m_index >= 0 && m_index < m_tracks.size() ? &m_tracks[m_index] : nullptr;
}
}
