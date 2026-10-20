#include "WikipediaSource.h"
#include "MusicBrainzSource.h"
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

namespace {
// Bound title fan-out and accepted extract size before exposing remote text to the UI.
constexpr auto MaximumSearchTitles = 6;
constexpr auto MaximumExtractCharacters = 8000;
}

using namespace Qt::StringLiterals;

namespace yalahoo {
QStringList wikipediaTitles(const Album &album)
{
    QStringList result{ album.title.trimmed() };
    static const QRegularExpression brackets(u"[\\(\uff08]([^()\uff08\uff09]+)[\\)\uff09]"_s);
    auto matches = brackets.globalMatch(album.title);
    auto outside = album.title;
    outside.remove(brackets);
    result.append(outside.trimmed());
    while (matches.hasNext())
        result.append(matches.next().captured(1).trimmed());
    result.removeAll(QString());
    result.removeDuplicates();
    return result.mid(0, MaximumSearchTitles);
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
        if (data.contains(u"id"_s))
            result.append(data.value(u"id"_s).toString());
        else if (data.contains(u"time"_s))
            result.append(data.value(u"time"_s).toString());
    }
    return result;
}
QStringList wikidataNames(const QJsonObject &entity)
{
    QStringList result;
    const auto labels = entity.value(u"labels"_s).toObject();
    for (const auto &value : labels)
        result.append(value.toObject().value(u"value"_s).toString());
    const auto aliases = entity.value(u"aliases"_s).toObject();
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
    for (const auto &title : wikipediaTitles(album))
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
    QSet<QString> matched;
    for (const auto &value : pages) {
        const auto page = value.toObject();
        const auto properties = page.value(u"pageprops"_s).toObject();
        if (page.contains(u"missing"_s) || properties.contains(u"disambiguation"_s))
            continue;
        const auto qid = properties.value(u"wikibase_item"_s).toString();
        const auto entity = entities.value(qid).toObject();
        auto names = wikidataNames(entity);
        names.append(page.value(u"title"_s).toString());
        auto base = page.value(u"title"_s).toString();
        base.remove(suffix);
        names.append(base);
        bool titleMatches = false;
        for (const auto &name : names)
            titleMatches |= titles.contains(metadataName(name));
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
            performersComplete &=
                    entities.contains(id) && !entities.value(id).toObject().contains(u"missing"_s);
            for (const auto &name : wikidataNames(entities.value(id).toObject()))
                performerMatches |= metadataName(name) == artist;
        }
        const auto dates = wikidataClaims(entity, u"P577"_s);
        bool yearMatches = false;
        if (album.year) {
            for (const auto &date : dates)
                yearMatches |= date.startsWith('+'_L1 + QString::number(*album.year) + '-'_L1);
            if (!dates.isEmpty() && !yearMatches)
                continue;
        }
        if (!performerIds.isEmpty() && (!performersComplete || !performerMatches))
            continue;
        // Text fallback requires both the artist and a release-year statement.
        bool textual = false;
        if (album.year && metadataName(first).contains(artist)) {
            const auto year = QString::number(*album.year);
            const QRegularExpression publication(
                    u"(?:"_s + year
                            + u"[^.!?\u3002\\n]{0,90}(?:released|published|\u53d1\u884c|\u767c\u884c|\u63a8\u51fa|\u51fa\u7248)|(?:released|published|\u53d1\u884c|\u767c\u884c|\u63a8\u51fa|\u51fa\u7248)[^.!?\u3002\\n]{0,90}"_s
                            + year + ')'_L1,
                    QRegularExpression::CaseInsensitiveOption);
            textual = first.contains(publication);
        }
        if (!(performerMatches && (!album.year || yearMatches)) && !textual)
            continue;
        const auto identity = qid.isEmpty() ? page.value(u"title"_s).toString() : qid;
        matched.insert(identity);
        if (matched.size() > 1) {
            result.status = ProviderStatus::Uncertain;
            result.payload = { };
            const auto revisions = page.value(u"revisions"_s).toArray();
            const auto revision = revisions.isEmpty()
                    ? QJsonValue()
                    : revisions.first().toObject().value(u"revid"_s);
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
