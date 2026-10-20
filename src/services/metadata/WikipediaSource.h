#pragma once
#include "IMetadataProvider.h"

namespace yalahoo {
QStringList wikipediaTitles(const Album &album);
QStringList wikidataClaims(const QJsonObject &entity, const QString &property);
QStringList wikidataNames(const QJsonObject &entity);
ProviderResult wikipediaMetadata(const Album &album, const QJsonArray &pages,
                                 const QJsonObject &entities, const QString &language);
}
