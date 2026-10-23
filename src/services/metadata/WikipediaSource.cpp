#include "WikipediaSource.h"
#include "MusicBrainzSource.h"
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <algorithm>

namespace {
// Bound title fan-out and accepted extract size before exposing remote text to the UI.
constexpr auto MaximumSearchTitles = 6;
constexpr auto MaximumExtractCharacters = 8000;
}

using namespace Qt::StringLiterals;

namespace yalahoo {
QString wikipediaEntityId(const QJsonObject &page)
{
    return page.value(u"pageprops"_s).toObject().value(u"wikibase_item"_s).toString();
}
QStringList wikipediaTitles(const Album &album)
{
    QStringList result;
    result.reserve(MaximumSearchTitles);
    const auto append = [&result](const QString &title) {
        const auto trimmed = title.trimmed();
        if (!trimmed.isEmpty() && !result.contains(trimmed))
            result.append(trimmed);
    };
    append(album.title);
    static const QRegularExpression brackets(u"[\\(\uff08]([^()\uff08\uff09]+)[\\)\uff09]"_s);
    auto matches = brackets.globalMatch(album.title);
    auto outside = album.title;
    outside.remove(brackets);
    append(outside);
    while (result.size() < MaximumSearchTitles && matches.hasNext())
        append(matches.next().captured(1));
    return result;
}
QStringList wikidataClaims(const QJsonObject &entity, const QString &property)
{
    QStringList result;
    for (const auto &value : entity.value(u"claims"_s).toObject().value(property).toArray()) {
        const auto claim = value.toObject();
        if (claim.value(u"rank"_s) == "deprecated"_L1)
            continue;
        const auto data = claim.value(u"mainsnak"_s)
                                  .toObject()
                                  .value(u"datavalue"_s)
                                  .toObject()
                                  .value(u"value"_s)
                                  .toObject();
        if (const auto id = data.constFind(u"id"_s); id != data.cend())
            result.append(id.value().toString());
        else if (const auto time = data.constFind(u"time"_s); time != data.cend())
            result.append(time.value().toString());
    }
    return result;
}
QStringList wikidataNames(const QJsonObject &entity)
{
    QStringList result;
    const auto labels = entity.value(u"labels"_s).toObject();
    const auto aliases = entity.value(u"aliases"_s).toObject();
    qsizetype count = labels.size();
    for (const auto &value : aliases)
        count += value.toArray().size();
    result.reserve(count);
    for (const auto &value : labels)
        result.append(value.toObject().value(u"value"_s).toString());
    for (const auto &value : aliases)
        for (const auto &alias : value.toArray())
            result.append(alias.toObject().value(u"value"_s).toString());
    return result;
}
ProviderResult wikipediaMetadata(const Album &album, const QJsonArray &pages,
                                 const QJsonObject &entities, const QString &language)
{
    ProviderResult result;
    result.kind = MetadataKind::Introduction;
    result.status = pages.isEmpty() ? ProviderStatus::NotFound : ProviderStatus::Uncertain;
    QSet<QString> titles;
    const auto searchTitles = wikipediaTitles(album);
    titles.reserve(searchTitles.size());
    for (const auto &title : searchTitles)
        titles.insert(metadataName(title));
    const auto artist = metadataName(album.artist);
    if (artist.isEmpty())
        return result;
    static const QRegularExpression suffix(
            u"\\s*[\\(\uff08][^()\uff08\uff09]*(?:album|EP|compilation|\u4e13\u8f91|\u5c08\u8f2f|\u5531\u7247)[\\)\uff09]\\s*$"_s,
            QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression albumWords(
            u"\\balbum\\b|\\bEP\\b|\\bcompilation\\b|\u4e13\u8f91|\u5c08\u8f2f|\u5531\u7247|\u5408\u8f91|\u5408\u8f2f"_s,
            QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression songWords(
            u"\\bis (?:an? |the )?[^.]{0,35}\\bsong\\b|(?:\u662f|\u70ba|\u4e3a)[^\u3002]{0,60}(?:\u4e00\u9996|\u6b4c\u66f2)"_s,
            QRegularExpression::CaseInsensitiveOption);
    const QString year = album.year ? QString::number(*album.year) : QString();
    const QString datePrefix = '+'_L1 + year + '-'_L1;
    const QRegularExpression publication(
            album.year ? u"(?:"_s + year
                            + u"[^.!?\u3002\\n]{0,90}(?:released|published|\u53d1\u884c|\u767c\u884c|\u63a8\u51fa|\u51fa\u7248)|(?:released|published|\u53d1\u884c|\u767c\u884c|\u63a8\u51fa|\u51fa\u7248)[^.!?\u3002\\n]{0,90}"_s
                            + year + ')'_L1
                       : QString(),
            QRegularExpression::CaseInsensitiveOption);
    QSet<QString> matched;
    for (const auto &value : pages) {
        const auto page = value.toObject();
        const auto properties = page.value(u"pageprops"_s).toObject();
        if (page.contains(u"missing"_s) || properties.contains(u"disambiguation"_s))
            continue;
        const auto qid = wikipediaEntityId(page);
        const auto entity = entities.value(qid).toObject();
        auto names = wikidataNames(entity);
        names.append(page.value(u"title"_s).toString());
        auto base = page.value(u"title"_s).toString();
        base.remove(suffix);
        names.append(base);
        const bool titleMatches = std::any_of(names.cbegin(), names.cend(), [&](const auto &name) {
            return titles.contains(metadataName(name));
        });
        if (!titleMatches)
            continue;
        const auto extract = page.value(u"extract"_s).toString().trimmed();
        const auto first = extract.section('\n'_L1, 0, 0);
        if (extract.isEmpty() || extract.size() > MaximumExtractCharacters
            || !first.contains(albumWords) || first.contains(songWords))
            continue;
        const auto performerIds = wikidataClaims(entity, u"P175"_s);
        bool performerMatches = false, performersComplete = true;
        for (const auto &id : performerIds) {
            const auto found = entities.constFind(id);
            const auto performer =
                    found == entities.cend() ? QJsonObject() : found.value().toObject();
            performersComplete &= found != entities.cend() && !performer.contains(u"missing"_s);
            if (!performerMatches) {
                const auto performerNames = wikidataNames(performer);
                performerMatches = std::any_of(performerNames.cbegin(), performerNames.cend(),
                                               [&](const auto &name) {
                                                   return metadataName(name) == artist;
                                               });
            }
        }
        const auto dates = wikidataClaims(entity, u"P577"_s);
        bool yearMatches = false;
        if (album.year) {
            yearMatches = std::any_of(dates.cbegin(), dates.cend(), [&](const auto &date) {
                return date.startsWith(datePrefix);
            });
            if (!dates.isEmpty() && !yearMatches)
                continue;
        }
        if (!performerIds.isEmpty() && (!performersComplete || !performerMatches))
            continue;
        // Text fallback requires both the artist and a release-year statement.
        bool textual = false;
        if (album.year && metadataName(first).contains(artist)) {
            textual = first.contains(publication);
        }
        if (!(performerMatches && (!album.year || yearMatches)) && !textual)
            continue;
        const auto identity = qid.isEmpty() ? page.value(u"title"_s).toString() : qid;
        matched.insert(identity);
        if (matched.size() > 1) {
            result.status = ProviderStatus::Uncertain;
            result.payload = { };
            result.source = { };
            return result;
        }
        const auto title = page.value(u"title"_s).toString();
        const auto url = u"https://"_s + language + u".wikipedia.org/wiki/"_s
                + QString::fromUtf8(
                                 QUrl::toPercentEncoding(QString(title).replace(' '_L1, '_'_L1)));
        result.status = ProviderStatus::Matched;
        result.payload = { { u"description"_s, extract } };
        const auto revisions = page.value(u"revisions"_s).toArray();
        const auto revision =
                revisions.isEmpty() ? QJsonValue() : revisions.first().toObject().value(u"revid"_s);
        result.source = { { u"name"_s, u"Wikipedia"_s },
                          { u"url"_s, url },
                          { u"license"_s, u"CC BY-SA 4.0"_s },
                          { u"licenseUrl"_s, u"https://creativecommons.org/licenses/by-sa/4.0/"_s },
                          { u"revisionId"_s, revision },
                          { u"revisionUrl"_s,
                            revision.isDouble() ? u"https://"_s + language
                                            + u".wikipedia.org/w/index.php?oldid="_s
                                            + QString::number(revision.toInteger())
                                                : QString() } };
    }
    return result;
}
}
