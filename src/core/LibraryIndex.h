// Owns an immutable library snapshot and stable-ID lookup tables. Borrowed records remain
// valid until replacement; metadata updates must preserve the indexed library structure.
#pragma once
#include "MusicTypes.h"
#include <QHash>

namespace yalahoo {
bool sameLibraryStructure(const LibrarySnapshot &before, const LibrarySnapshot &after);
class LibraryIndex final
{
public:
    LibraryIndex();
    void replace(Snapshot snapshot);
    [[nodiscard]] bool replaceMetadata(Snapshot snapshot);
    void setMetadataConfiguration(const QJsonValue &configuration);
    Snapshot snapshot() const;
    const Album *album(const QString &id) const;
    const Track *track(const QString &albumId, const QString &trackId) const;
    int trackIndex(const QString &albumId, const QString &trackId) const;

private:
    Snapshot m_snapshot;
    QHash<QString, qsizetype> m_albumIndices;
    QHash<QString, QHash<QString, qsizetype>> m_trackIndices;
};
}
