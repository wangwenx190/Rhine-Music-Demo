#pragma once
#include "IMetadataProvider.h"

namespace yalahoo {
QString metadataName(const QString &text);
QJsonArray musicBrainzCandidates(const Album &album, const QJsonObject &search);
QString musicBrainzRelease(const Album &album, const QJsonObject &search);
ProviderResult musicBrainzMetadata(const Album &album, MetadataKind kind,
                                   const QJsonObject &release);
}
