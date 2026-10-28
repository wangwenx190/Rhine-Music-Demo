// Merges accepted provider values into immutable library candidates. Stable album/request
// identity and provenance prevent stale responses from overwriting current data.

#pragma once
#include "IMetadataProvider.h"
#include <QHash>

namespace yalahoo {
// Only metadata fields are changed; local tags, paths, identity and classification survive.
ProviderResult cachedMetadata(const Album &album, MetadataKind kind, const RequestToken &token);
QString metadataKey(const Album &album, MetadataKind kind);
bool mergeMetadata(Album &album, const ProviderResult &result);
Snapshot mergeMetadataBatch(Snapshot snapshot, const QHash<QString, ProviderResult> &pending);
}
