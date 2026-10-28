// Defines transport-sensitive cache reuse and timestamp/source publication for provider results.
// A caller-supplied UTC clock makes expiry and retry boundaries independent of HTTP scheduling.

#pragma once
#include "IMetadataProvider.h"
#include <QDateTime>
#include <optional>

namespace yalahoo {
QString preferredRelease(const AlbumQuery &query);
std::optional<ProviderResult> reusableMetadata(const AlbumQuery &query, const RequestToken &token,
                                               MetadataKind kind, const QString &inputKey,
                                               const QDateTime &now);
void stampMetadata(ProviderResult &result, const AlbumQuery &query, const QString &releaseId,
                   const QDateTime &now);
}
