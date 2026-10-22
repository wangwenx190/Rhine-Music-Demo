#include "MetadataWorker.h"
#include "MetadataCachePolicy.h"
#include "MetadataMerge.h"
#include "MusicBrainzSource.h"
#include "WikipediaSource.h"
#include <QSet>
#include <QDateTime>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUrlQuery>
#include <utility>

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
// Limits bound remote response expansion; cache and retry ages are measured in UTC.
constexpr auto MaximumQueryFieldCharacters = 512;
constexpr auto MaximumCandidatePages = 12;
constexpr auto MaximumPerformers = 50;

struct QueryParameter final
{
    QString name, value;
};
QUrl endpoint(const QString &base, const QList<QueryParameter> &parameters)
{
    QUrl url(base);
    QUrlQuery query;
    for (const auto &parameter : parameters)
        query.addQueryItem(parameter.name, parameter.value);
    url.setQuery(query);
    return url;
}
bool validMbid(const QString &id)
{
    static const QRegularExpression pattern(
            u"^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"_s);
    return pattern.match(id).hasMatch();
}
QString escapeSearch(const QString &value)
{
    QString result;
    result.reserve(value.size() * 2);
    static constexpr auto reserved = "+-&|!(){}[]^\"~*?:\\/"_L1;
    for (const auto character : value) {
        if (reserved.contains(character))
            result += '\\'_L1;
        result += character;
    }
    return result;
}

}
MetadataWorker::MetadataWorker(QNetworkAccessManager *network) : m_http(this, network), m_next(this)
{
    m_next.setSingleShot(true);
    connect(&m_next, &QTimer::timeout, this, &MetadataWorker::start);
    connect(&m_http, &MetadataHttp::completed, this, &MetadataWorker::receive);
}

void MetadataWorker::enqueue(AlbumQuery query, RequestToken token, MetadataKind kind)
{
    if (m_waiting.size() + int(m_active.has_value())
        >= yalahoo::metadataLimits::MaximumPendingRequests)
        return;
    Job job;
    job.query = std::move(query);
    job.token = std::move(token);
    job.result.kind = kind;
    m_waiting.append(std::move(job));
    if (!m_active)
        m_next.start(0);
}

void MetadataWorker::cancel(const RequestToken &token)
{
    const bool waiting = m_waiting.removeIf([&](const Job &job) {
        return job.token == token;
    }) > 0;
    const bool active = m_active && m_active->token == token;
    if (active) {
        m_http.cancel();
        m_active.reset();
    }
    if (waiting || active) {
        ProviderResult result;
        result.requestId = token.requestId;
        result.generation = token.generation;
        result.albumId = token.albumId;
        result.status = ProviderStatus::Cancelled;
        Q_EMIT ready(result);
    }
    if (!m_active && !m_waiting.isEmpty())
        m_next.start(0);
    else if (!m_active)
        m_next.stop();
}

void MetadataWorker::stop()
{
    m_next.stop();
    m_waiting.clear();
    m_http.cancel();
    m_active.reset();
}

void MetadataWorker::start()
{
    if (m_active || m_waiting.isEmpty())
        return;
    m_active = m_waiting.takeFirst();
    auto &job = *m_active;
    const auto &album = job.query.album;
    const auto kind = job.result.kind;
    // The job owns an immutable album query throughout all HTTP stages.
    job.inputKey = metadataKey(album, kind);
    if (auto cached = reusableMetadata(job.query, job.token, kind, job.inputKey,
                                       QDateTime::currentDateTimeUtc())) {
        job.result = std::move(*cached);
        finish(false);
        return;
    }
    if (!job.query.enabled) {
        job.result.status = ProviderStatus::Deferred;
        job.result.error = tr("Enable online metadata in settings to query this source.");
        finish(false);
        return;
    }
    if (album.id != job.token.albumId || job.query.albumId != job.token.albumId
        || album.title.trimmed().isEmpty() || album.artist.trimmed().isEmpty()) {
        job.result.status = ProviderStatus::Uncertain;
        job.result.error = tr("An album title and artist are required for a reliable match.");
        finish(false);
        return;
    }
    if (album.title.size() > MaximumQueryFieldCharacters
        || album.artist.size() > MaximumQueryFieldCharacters) {
        job.result.status = ProviderStatus::Uncertain;
        job.result.error = tr(
                "Album titles and artist names must be at most 512 characters for online queries.");
        finish(false);
        return;
    }
    if (kind == MetadataKind::Introduction) {
        pages(false);
        return;
    }
    const auto id = preferredRelease(job.query);
    if (!id.isEmpty() && !validMbid(id)) {
        fail(tr("Invalid MusicBrainz release identifier."));
        return;
    }
    if (!id.isEmpty()) {
        release(id);
        return;
    }
    job.stage = Stage::ReleaseSearch;
    get(endpoint(u"https://musicbrainz.org/ws/2/release/"_s,
                 { { u"fmt"_s, u"json"_s },
                   { u"limit"_s, u"10"_s },
                   { u"query"_s,
                     u"release:\""_s + escapeSearch(album.title) + u"\" AND artist:\""_s
                             + escapeSearch(album.artist) + '\"'_L1 } }));
}

void MetadataWorker::get(const QUrl &url)
{
    const auto &query = m_active->query;
    m_http.get(url, query.contact, query.configuration);
}

void MetadataWorker::release(const QString &id)
{
    m_active->stage = Stage::Release;
    m_active->releaseId = id;
    const auto includes = m_active->result.kind == MetadataKind::Credits
            ? u"artist-credits+recordings+recording-rels+recording-level-rels+work-rels+work-level-rels+artist-rels+release-groups"_s
            : u"artist-credits+recordings+release-groups+genres"_s;
    get(endpoint(u"https://musicbrainz.org/ws/2/release/"_s + id,
                 { { u"fmt"_s, u"json"_s }, { u"inc"_s, includes } }));
}

void MetadataWorker::pages(bool search)
{
    auto &job = *m_active;
    job.stage = Stage::Pages;
    job.searched = search;
    QList<QueryParameter> parameters{ { u"action"_s, u"query"_s },
                                      { u"format"_s, u"json"_s },
                                      { u"formatversion"_s, u"2"_s },
                                      { u"maxlag"_s, u"5"_s },
                                      { u"prop"_s, u"extracts|pageprops|revisions"_s },
                                      { u"exintro"_s, u"1"_s },
                                      { u"explaintext"_s, u"1"_s },
                                      { u"exchars"_s, u"1200"_s },
                                      { u"rvprop"_s, u"ids"_s },
                                      { u"redirects"_s, u"1"_s },
                                      { u"converttitles"_s, u"1"_s } };
    if (search) {
        parameters.append({ u"generator"_s, u"search"_s });
        auto title = job.query.album.title;
        auto artist = job.query.album.artist;
        title.remove('"'_L1);
        artist.remove('"'_L1);
        parameters.append({ u"gsrsearch"_s, '"'_L1 + title + u"\" \""_s + artist + '"'_L1 });
        parameters.append({ u"gsrlimit"_s, u"5"_s });
        parameters.append({ u"gsrnamespace"_s, u"0"_s });
    } else {
        parameters.append({ u"titles"_s, wikipediaTitles(job.query.album).join('|'_L1) });
    }
    get(endpoint(u"https://"_s + job.language + u".wikipedia.org/w/api.php"_s, parameters));
}

void MetadataWorker::entities(const QStringList &ids, Stage stage)
{
    m_active->stage = stage;
    get(endpoint(u"https://www.wikidata.org/w/api.php"_s,
                 { { u"action"_s, u"wbgetentities"_s },
                   { u"format"_s, u"json"_s },
                   { u"maxlag"_s, u"5"_s },
                   { u"ids"_s, ids.join('|'_L1) },
                   { u"props"_s, u"labels|aliases|claims"_s },
                   { u"languages"_s, u"zh|zh-hans|zh-hant|zh-cn|zh-tw|en"_s },
                   { u"languagefallback"_s, u"1"_s } }));
}

void MetadataWorker::assess()
{
    auto &job = *m_active;
    auto result = wikipediaMetadata(job.query.album, job.pages, job.entities, job.language);
    if (result.status == ProviderStatus::Matched) {
        job.result = std::move(result);
        finish();
        return;
    }
    job.uncertain |= result.status == ProviderStatus::Uncertain;
    if (!job.searched) {
        pages(true);
    } else if (job.language == "zh"_L1) {
        job.language = u"en"_s;
        job.pages = { };
        job.entities = { };
        pages(false);
    } else {
        job.result.status = !job.failures.isEmpty() ? ProviderStatus::Error
                : job.uncertain                     ? ProviderStatus::Uncertain
                                                    : ProviderStatus::NotFound;
        job.result.error = job.failures.join('\n'_L1);
        finish();
    }
}

void MetadataWorker::receive(const MetadataResponse &response)
{
    if (!m_active)
        return;
    auto &job = *m_active;
    if (!response.error.isEmpty()) {
        job.result.retryAt = response.retryAt;
        if (job.result.kind != MetadataKind::Introduction) {
            fail(response.error);
            return;
        }
        job.failures.append(response.error);
        if (job.stage == Stage::Pages)
            job.pages = { };
        assess();
        return;
    }
    const auto &data = response.data;
    static constexpr void (MetadataWorker::*handlers[])(const QJsonObject &) = {
        &MetadataWorker::receiveSearch,   &MetadataWorker::receiveRelease,
        &MetadataWorker::receiveGroup,    &MetadataWorker::receivePages,
        &MetadataWorker::receiveEntities, &MetadataWorker::receiveEntities
    };
    (this->*handlers[std::to_underlying(job.stage)])(data);
}

void MetadataWorker::receiveSearch(const QJsonObject &data)
{
    auto &job = *m_active;
    if (!data.value(u"releases"_s).isArray() || !data.value(u"count"_s).isDouble()
        || data.value(u"count"_s).toInteger(-1) < 0) {
        fail(tr("MusicBrainz returned an unexpected search response."));
        return;
    }
    const auto id = musicBrainzRelease(job.query.album, data);
    if (!validMbid(id)) {
        job.result.status = data.value(u"releases"_s).toArray().isEmpty()
                ? ProviderStatus::NotFound
                : ProviderStatus::Uncertain;
        const auto field = metadataStateKey(job.result.kind);
        job.result.payload.insert(
                field,
                QJsonObject{ { u"candidates"_s, musicBrainzCandidates(job.query.album, data) } });

        finish();
        return;
    }
    release(id);
}

void MetadataWorker::receiveRelease(const QJsonObject &data)
{
    auto &job = *m_active;
    if (!data.value(u"media"_s).isArray() || data.value(u"id"_s).toString() != job.releaseId) {
        fail(tr("MusicBrainz returned an unexpected release response."));
        return;
    }
    const auto match = musicBrainzMetadata(job.query.album, job.result.kind, data);
    if (match.status == ProviderStatus::Uncertain) {
        job.result = match;
        finish();
        return;
    }
    job.release = data;
    const auto group = data.value(u"release-group"_s).toObject().value(u"id"_s).toString();
    if (job.result.kind == MetadataKind::Enrichment && validMbid(group)) {
        job.stage = Stage::Group;
        job.groupId = group;
        get(endpoint(u"https://musicbrainz.org/ws/2/release-group/"_s + group,
                     { { u"fmt"_s, u"json"_s }, { u"inc"_s, u"genres"_s } }));
    } else {
        job.result = match;
        finish();
    }
}

void MetadataWorker::receiveGroup(const QJsonObject &data)
{
    auto &job = *m_active;
    if (data.value(u"id"_s).toString() != job.groupId) {
        fail(tr("MusicBrainz returned an unexpected release group response."));
        return;
    }
    job.release.insert(u"release-group"_s, data);
    job.result = musicBrainzMetadata(job.query.album, job.result.kind, job.release);
    finish();
}

void MetadataWorker::receivePages(const QJsonObject &data)
{
    auto &job = *m_active;
    const auto query = data.value(u"query"_s).toObject();
    const bool emptySearch = data.contains(u"batchcomplete"_s)
            && (query.isEmpty()
                || query.value(u"searchinfo"_s).toObject().value(u"totalhits"_s).toInt(-1) == 0);
    if (!query.value(u"pages"_s).isArray() && !emptySearch) {
        job.failures.append(tr("Wikipedia returned an unexpected page response."));
        assess();
        return;
    }
    const auto incoming = query.value(u"pages"_s).toArray();
    QSet<qint64> seen;
    for (const auto &page : std::as_const(job.pages))
        seen.insert(page.toObject().value(u"pageid"_s).toInteger());
    for (const auto &page : incoming) {
        const auto object = page.toObject();
        if (object.contains(u"missing"_s) || object.value(u"pageid"_s).toInteger() <= 0)
            continue;
        if (!seen.contains(object.value(u"pageid"_s).toInteger())
            && job.pages.size() < MaximumCandidatePages) {
            seen.insert(object.value(u"pageid"_s).toInteger());
            job.pages.append(page);
        }
    }
    QStringList ids;
    static const QRegularExpression qid(u"^Q[1-9][0-9]*$"_s);
    for (const auto &page : std::as_const(job.pages)) {
        const auto id = page.toObject()
                                .value(u"pageprops"_s)
                                .toObject()
                                .value(u"wikibase_item"_s)
                                .toString();
        if (qid.match(id).hasMatch() && !job.entities.contains(id))
            ids.append(id);
    }
    ids.removeDuplicates();
    if (ids.isEmpty())
        assess();
    else
        entities(ids, Stage::Entities);
}

void MetadataWorker::receiveEntities(const QJsonObject &data)
{
    auto &job = *m_active;
    if (!data.value(u"entities"_s).isObject()) {
        job.failures.append(tr("Wikidata returned an unexpected entity response."));
        assess();
        return;
    }
    const auto incoming = data.value(u"entities"_s).toObject();
    for (auto it = incoming.begin(); it != incoming.end(); ++it)
        job.entities.insert(it.key(), it.value());
    QStringList performers;
    if (job.stage == Stage::Entities) {
        for (const auto &page : std::as_const(job.pages)) {
            const auto id = page.toObject()
                                    .value(u"pageprops"_s)
                                    .toObject()
                                    .value(u"wikibase_item"_s)
                                    .toString();
            for (const auto &performer :
                 wikidataClaims(job.entities.value(id).toObject(), u"P175"_s))
                if (!job.entities.contains(performer))
                    performers.append(performer);
        }
        performers.removeDuplicates();
    }
    if (performers.size() > MaximumPerformers) {
        job.uncertain = true;
        assess();
    } else if (performers.isEmpty()) {
        assess();
    } else {
        entities(performers, Stage::Performers);
    }
}

void MetadataWorker::fail(const QString &message)
{
    m_active->result.status = ProviderStatus::Error;
    m_active->result.error = message;
    finish();
}

void MetadataWorker::finish(bool checked)
{
    auto job = std::move(*m_active);
    m_active.reset();
    auto &result = job.result;
    result.requestId = job.token.requestId;
    result.generation = job.token.generation;
    result.albumId = job.token.albumId;
    result.inputKey = std::move(job.inputKey);
    if (checked)
        stampMetadata(result, job.query, job.releaseId, QDateTime::currentDateTimeUtc());
    result.payloadBytes = QJsonDocument(result.payload).toJson(QJsonDocument::Compact).size();
    m_next.start(0);
    Q_EMIT ready(result);
}
}
