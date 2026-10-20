#pragma once
#include "IMetadataProvider.h"

namespace yalahoo {
// Only metadata fields are changed; local tags, paths, identity and classification survive.
ProviderResult cachedMetadata(const Album &album, MetadataKind kind, RequestToken token);
QString metadataKey(const Album &album, MetadataKind kind);
bool mergeMetadata(Album &album, const ProviderResult &result);
}
