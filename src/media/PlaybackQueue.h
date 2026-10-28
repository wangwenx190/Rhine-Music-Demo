// Retains one album's deduplicated track values independently of library replacement.
// Selection validates a candidate before committing queue, index and weak snapshot identity.

#pragma once
#include "MusicTypes.h"
#include <QHash>

namespace yalahoo {
class PlaybackQueue final
{
public:
    enum class Selection {
        Accepted,
        MissingTrack,
        InvalidSource
    };
    Selection select(const Snapshot &snapshot, const Album &album, const QString &trackId);
    bool selectIndex(int index);
    const Track *current() const;
    int index() const
    {
        return m_index;
    }
    bool hasNext() const
    {
        return m_index >= 0 && m_index + 1 < m_tracks.size();
    }
    const QList<Track> &tracks() const
    {
        return m_tracks;
    }

private:
    QList<Track> m_tracks;
    QHash<QString, int> m_indices;
    std::weak_ptr<const LibrarySnapshot> m_snapshot;
    QString m_albumId;
    int m_index = -1;
};
}
