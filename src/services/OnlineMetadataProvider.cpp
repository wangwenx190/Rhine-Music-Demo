#include "IMetadataProvider.h"
#include "OnlineMetadataProvider.h"
#include <QThread>
#include <QSet>
#include <QHash>
#include "metadata/MetadataHttp.h"
#include "metadata/MetadataMerge.h"
#include "metadata/MusicBrainzSource.h"
#include "metadata/WikipediaSource.h"
#include <QDateTime>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QNetworkProxyFactory>
#include <QRegularExpression>
#include <QSet>
#include <QUrlQuery>
#include <optional>
#include <utility>

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
// Limits bound remote response expansion; cache and retry ages are measured in UTC.
constexpr auto PositiveCacheDays = 30;
constexpr auto MaximumQueryFieldCharacters = 512;
constexpr auto MaximumCandidatePages = 12;
constexpr auto MaximumPerformers = 50;
constexpr auto RetryDelaySeconds = 60;

QUrl endpoint(const QString &base, const QList<QPair<QString, QString>> &parameters)
{
    QUrl url(base);
    QUrlQuery query;
    for (const auto &parameter : parameters)
        query.addQueryItem(parameter.first, parameter.second);
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
    const auto reserved = u"+-&|!(){}[]^\"~*?:\\/"_s;
    for (const auto character : value) {
        if (reserved.contains(character))
            result += '\\'_L1;
        result += character;
    }
    return result;
}
QString transportKey(const AlbumQuery &query)
{
    const auto &configuration = query.configuration;
    const auto bytes =
            QJsonDocument(QJsonArray{ configuration.value(u"proxyEnabled"_s).toBool(true),
                                      configuration.value(u"proxyMode"_s).toString(u"system"_s),
                                      configuration.value(u"proxyUrl"_s).toString(),
                                      query.contact })
                    .toJson(QJsonDocument::Compact);
    return QString::fromUtf8(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
QString preferredRelease(const AlbumQuery &query)
{
    if (!query.releaseId.isEmpty())
        return query.releaseId;
    const auto credits = query.album.creditsLookup;
    const auto online = query.album.online;
    const auto creditsId = credits.value(u"releaseId"_s).toString();
    const auto onlineId = online.value(u"releaseId"_s).toString();
    const auto creditsTime =
            QDateTime::fromString(credits.value(u"checkedAt"_s).toString(), Qt::ISODateWithMs);
    const auto onlineTime =
            QDateTime::fromString(online.value(u"checkedAt"_s).toString(), Qt::ISODateWithMs);
    return !creditsId.isEmpty()
                    && (onlineId.isEmpty()
                        || (creditsTime.isValid()
                            && (!onlineTime.isValid() || creditsTime > onlineTime)))
            ? creditsId
            : onlineId;
}
QString stateName(ProviderStatus status)
{
    return status == ProviderStatus::NotFound ? u"not-found"_s : statusName(status).toLower();
}
}
class MetadataWorker final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MetadataWorker)
public:
    explicit MetadataWorker(QNetworkAccessManager *network) : m_http(this, network), m_next(this)
    {
        m_next.setSingleShot(true);
        connect(&m_next, &QTimer::timeout, this, &MetadataWorker::start);
        connect(&m_http, &MetadataHttp::completed, this, &MetadataWorker::receive);
    }
    void enqueue(AlbumQuery query, RequestToken token, MetadataKind kind)
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
    void cancel(const RequestToken &token)
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
    void stop()
    {
        m_next.stop();
        m_waiting.clear();
        m_http.cancel();
        m_active.reset();
    }
Q_SIGNALS:
    void ready(const yalahoo::ProviderResult &result);

private:
    enum class Stage {
        ReleaseSearch,
        Release,
        Group,
        Pages,
        Entities,
        Performers
    };
    struct Job final
    {
        AlbumQuery query;
        RequestToken token;
        ProviderResult result;
        Stage stage = Stage::ReleaseSearch;
        QJsonObject release, entities;
        QJsonArray pages;
        QStringList failures;
        bool uncertain = false, searched = false;
        QString language = u"zh"_s;
        QString releaseId, groupId;
    };
    void start()
    {
        if (m_active || m_waiting.isEmpty())
            return;
        m_active = m_waiting.takeFirst();
        auto &job = *m_active;
        const auto &album = job.query.album;
        const auto kind = job.result.kind;
        const auto key = metadataKey(album, kind);
        const auto cached = cachedMetadata(album, kind, job.token);
        const auto state = kind == MetadataKind::Introduction ? album.introduction
                : kind == MetadataKind::Credits               ? album.creditsLookup
                                                              : album.online;
        const auto expires =
                QDateTime::fromString(state.value(u"expiresAt"_s).toString(), Qt::ISODateWithMs);
        const auto retry = QDateTime::fromString(cached.retryAt, Qt::ISODateWithMs);
        const auto desiredRelease =
                kind == MetadataKind::Introduction ? QString() : preferredRelease(job.query);
        const bool same =
                (!state.contains(u"inputKey"_s) || state.value(u"inputKey"_s).toString() == key)
                && (desiredRelease.isEmpty()
                    || state.value(u"releaseId"_s).toString() == desiredRelease);
        const auto checked = QDateTime::fromString(cached.checkedAt, Qt::ISODateWithMs);
        const bool positive = cached.status == ProviderStatus::Matched
                || cached.status == ProviderStatus::Partial;
        const auto expiry = expires.isValid() ? expires
                : checked.isValid()           ? checked.addDays(positive ? PositiveCacheDays : 1)
                                              : QDateTime();
        const bool fresh = expiry.isValid() ? expiry > QDateTime::currentDateTimeUtc() : positive;
        const bool hasCache = kind != MetadataKind::Credits || !state.isEmpty();
        const bool sameTransport = cached.status != ProviderStatus::Error
                || state.value(u"transportKey"_s).toString() == transportKey(job.query);
        if (same && (sameTransport || !job.query.enabled) && (hasCache || !job.query.enabled)
            && cached.status != ProviderStatus::Deferred
            && ((!job.query.force && fresh) || !job.query.enabled
                || retry > QDateTime::currentDateTimeUtc())) {
            job.result = cached;
            job.result.fromCache = true;
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

    void get(const QUrl &url)
    {
        const auto &query = m_active->query;
        m_http.get(url, query.contact, query.configuration);
    }
    void release(const QString &id)
    {
        m_active->stage = Stage::Release;
        m_active->releaseId = id;
        const auto includes = m_active->result.kind == MetadataKind::Credits
                ? u"artist-credits+recordings+recording-rels+recording-level-rels+work-rels+work-level-rels+artist-rels+release-groups"_s
                : u"artist-credits+recordings+release-groups+genres"_s;
        get(endpoint(u"https://musicbrainz.org/ws/2/release/"_s + id,
                     { { u"fmt"_s, u"json"_s }, { u"inc"_s, includes } }));
    }
    void pages(bool search)
    {
        auto &job = *m_active;
        job.stage = Stage::Pages;
        job.searched = search;
        QList<QPair<QString, QString>> parameters{ { u"action"_s, u"query"_s },
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
    void entities(const QStringList &ids, Stage stage)
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
    void assess()
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
    void receive(const MetadataResponse &response)
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
    void receiveSearch(const QJsonObject &data)
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
            const auto field =
                    job.result.kind == MetadataKind::Credits ? u"creditsLookup"_s : u"online"_s;
            job.result.payload.insert(
                    field,
                    QJsonObject{
                            { u"candidates"_s, musicBrainzCandidates(job.query.album, data) } });

            finish();
            return;
        }
        release(id);
    }
    void receiveRelease(const QJsonObject &data)
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
    void receiveGroup(const QJsonObject &data)
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
    void receivePages(const QJsonObject &data)
    {
        auto &job = *m_active;
        const auto query = data.value(u"query"_s).toObject();
        const bool emptySearch = data.contains(u"batchcomplete"_s)
                && (query.isEmpty()
                    || query.value(u"searchinfo"_s).toObject().value(u"totalhits"_s).toInt(-1)
                            == 0);
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
    void receiveEntities(const QJsonObject &data)
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

    void fail(const QString &message)
    {
        m_active->result.status = ProviderStatus::Error;
        m_active->result.error = message;
        finish();
    }
    void finish(bool checked = true)
    {
        auto job = std::move(*m_active);
        m_active.reset();
        auto &result = job.result;
        result.requestId = job.token.requestId;
        result.generation = job.token.generation;
        result.albumId = job.token.albumId;
        result.inputKey = metadataKey(job.query.album, result.kind);
        if (checked) {
            const auto now = QDateTime::currentDateTimeUtc();
            result.checkedAt = now.toString(Qt::ISODateWithMs);
            if (result.status == ProviderStatus::Error && result.retryAt.isEmpty())
                result.retryAt = now.addSecs(RetryDelaySeconds).toString(Qt::ISODateWithMs);
            const auto field = result.kind == MetadataKind::Introduction ? u"introduction"_s
                    : result.kind == MetadataKind::Credits               ? u"creditsLookup"_s
                                                                         : u"online"_s;
            auto state = result.kind == MetadataKind::Introduction ? job.query.album.introduction
                    : result.kind == MetadataKind::Credits         ? job.query.album.creditsLookup
                                                                   : job.query.album.online;
            const auto patch = result.payload.value(field).toObject();
            for (auto it = patch.begin(); it != patch.end(); ++it)
                state.insert(it.key(), it.value());
            if (!patch.contains(u"candidates"_s))
                state.remove(u"candidates"_s);
            if (result.source.isEmpty())
                result.source = { { u"name"_s,
                                    result.kind == MetadataKind::Introduction
                                            ? u"Wikipedia / Wikidata"_s
                                            : u"MusicBrainz"_s } };
            if (result.kind != MetadataKind::Introduction && !job.releaseId.isEmpty())
                state.insert(u"releaseId"_s, job.releaseId);
            state.insert(u"source"_s, result.source.value(u"name"_s));
            state.insert(u"sourceDetails"_s, result.source);
            if (result.source.contains(u"url"_s))
                state.insert(u"sourceUrl"_s, result.source.value(u"url"_s));
            state.insert(u"status"_s, stateName(result.status));
            state.insert(u"checkedAt"_s, result.checkedAt);
            state.insert(u"retryAt"_s, result.retryAt);
            state.insert(u"error"_s, result.error);
            state.insert(u"inputKey"_s, result.inputKey);
            if (result.status == ProviderStatus::Error)
                state.insert(u"transportKey"_s, transportKey(job.query));
            else
                state.remove(u"transportKey"_s);
            state.insert(u"expiresAt"_s,
                         result.status == ProviderStatus::Error
                                 ? result.retryAt
                                 : now.addDays(result.status == ProviderStatus::Matched
                                                               || result.status
                                                                       == ProviderStatus::Partial
                                                       ? PositiveCacheDays
                                                       : 1)
                                           .toString(Qt::ISODateWithMs));
            result.payload.insert(field, state);
            if (!result.source.isEmpty())
                result.source.insert(u"checkedAt"_s, result.checkedAt);
        }
        result.payloadBytes = QJsonDocument(result.payload).toJson(QJsonDocument::Compact).size();
        m_next.start(0);
        Q_EMIT ready(result);
    }
    MetadataHttp m_http;
    QTimer m_next;
    QList<Job> m_waiting;
    std::optional<Job> m_active;
};
struct OnlineMetadataProvider::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    Private() = default;
    QThread m_thread;
    MetadataWorker *m_worker;
    QHash<RequestToken, MetadataKind> m_pending;
    QSet<RequestToken> m_outstanding;
    bool m_enabled = false;
};
OnlineMetadataProvider::OnlineMetadataProvider(
        QObject *parent, std::function<QNetworkAccessManager *()> networkFactory)
    : IMetadataProvider(parent), d(std::make_unique<Private>())
{
    d->m_worker = new MetadataWorker(networkFactory ? networkFactory() : nullptr);
    qRegisterMetaType<ProviderResult>();
    d->m_worker->moveToThread(&d->m_thread);
    connect(&d->m_thread, &QThread::finished, d->m_worker, &QObject::deleteLater);
    connect(d->m_worker, &MetadataWorker::ready, this, [this](const ProviderResult &result) {
        const RequestToken token{ result.requestId, result.generation, result.albumId };
        d->m_outstanding.remove(token);
        const auto it = d->m_pending.find(token);
        if (it == d->m_pending.end() || *it != result.kind)
            return;
        d->m_pending.erase(it);
        Q_EMIT resultReady(result);
    });
    d->m_thread.setObjectName(u"metadata-network"_s);
    d->m_thread.start();
}
OnlineMetadataProvider::~OnlineMetadataProvider()
{
    QMetaObject::invokeMethod(
            d->m_worker,
            [worker = d->m_worker] {
                worker->stop();
                QThread::currentThread()->quit();
            },
            Qt::QueuedConnection);
    d->m_thread.wait();
}
void OnlineMetadataProvider::configure(const QJsonObject &configuration)
{
    const bool enabled = configuration.value(u"networkEnabled"_s).toBool();
    if (enabled == d->m_enabled)
        return;
    d->m_enabled = enabled;
    const auto pending = d->m_pending;
    for (auto it = pending.begin(); it != pending.end(); ++it) {
        const auto token = it.key();
        cancel(token);
        ProviderResult result;
        result.requestId = token.requestId;
        result.generation = token.generation;
        result.albumId = token.albumId;
        result.kind = it.value();
        result.status = ProviderStatus::Cancelled;
        Q_EMIT resultReady(result);
    }
    Q_EMIT capabilitiesChanged();
}
ProviderCapabilities OnlineMetadataProvider::capabilities() const
{
    return { d->m_enabled, true, true, true, true };
}
// Reserve capacity in the GUI facade before posting to the network thread.
// The token remains the acceptance key even if cancellation races with a queued reply.
bool OnlineMetadataProvider::request(AlbumQuery query, RequestToken token, MetadataKind kind)
{
    if (d->m_pending.contains(token))
        return d->m_pending.value(token) == kind;
    if (d->m_outstanding.size() >= yalahoo::metadataLimits::MaximumPendingRequests
        || d->m_outstanding.contains(token))
        return false;
    d->m_outstanding.insert(token);
    query.enabled = d->m_enabled && query.enabled;
    d->m_pending.insert(token, kind);
    QMetaObject::invokeMethod(
            d->m_worker,
            [worker = d->m_worker, query = std::move(query), token = std::move(token),
             kind]() mutable {
                worker->enqueue(std::move(query), std::move(token), kind);
            },
            Qt::QueuedConnection);
    return true;
}
void OnlineMetadataProvider::cancel(RequestToken token)
{
    if (!d->m_pending.remove(token))
        return;
    QMetaObject::invokeMethod(
            d->m_worker,
            [worker = d->m_worker, token = std::move(token)] {
                worker->cancel(token);
            },
            Qt::QueuedConnection);
}
}
#include "OnlineMetadataProvider.moc"
