// Extracts Wikipedia and Wikidata identities, claims and titles. Pure parsing retains provider
// provenance without issuing network requests.

#pragma once
#include "IMetadataProvider.h"

namespace yalahoo {
inline constexpr int MaximumWikipediaExtractCharacters = 1200;
QString wikipediaEntityId(const QJsonObject &page);
QStringList wikipediaTitles(const Album &album);
QStringList wikidataClaims(const QJsonObject &entity, const QString &property);
QStringList wikidataNames(const QJsonObject &entity);
ProviderResult wikipediaMetadata(const Album &album, const QJsonArray &pages,
                                 const QJsonObject &entities, const QString &language);
}
