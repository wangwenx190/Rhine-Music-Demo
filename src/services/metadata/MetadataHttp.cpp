#include "MetadataHttp.h"
#include <memory>
#include <QElapsedTimer>
#include <QUrl>
#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QPointer>
#include <QTimer>
#include <utility>
#include "MetadataConfiguration.h"
#include <QNetworkAccessManager>
#include <algorithm>
#include <QDateTime>
#include <QJsonDocument>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
// Host pacing and a hard deadline bound each request; HTTP ranges use exclusive upper bounds.
constexpr auto HostSpacingMilliseconds = 1100;
constexpr auto TransferTimeoutMilliseconds = 12000;
constexpr auto RequestDeadlineMilliseconds = 15000;
constexpr auto HttpSuccessBegin = 200;
constexpr auto HttpSuccessEnd = 300;
constexpr auto HttpTooManyRequests = 429;
constexpr auto HttpUnavailable = 503;
constexpr auto MaximumRetryDays = 7;
constexpr auto HoursPerDay = 24;
constexpr auto SecondsPerHour = 3600;
constexpr auto FallbackRetrySeconds = 60;

constexpr qint64 maximumResponseBytes = 2 * 1024 * 1024;
QString service(const QUrl &url)
{
    return url.host() == "musicbrainz.org"_L1 ? u"musicbrainz.org"_s : u"wikimedia"_s;
}
}
struct MetadataHttp::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    explicit Private(MetadataHttp *owner, QNetworkAccessManager *network)
        : q(owner),
          m_network(network ? network : new QNetworkAccessManager(owner)),
          m_delay(owner),
          m_deadline(owner)
    {
    }
    MetadataHttp *const q;

    void parseResponse(MetadataResponse &response);
    void start();
    void finish();
    void read();
    QNetworkAccessManager *m_network;
    QPointer<QNetworkReply> m_reply;
    QTimer m_delay, m_deadline;
    QElapsedTimer m_clock;
    QHash<QString, qint64> m_next;
    QHash<QString, QDateTime> m_cooldown;
    QUrl m_url;
    QString m_contact, m_error;
    QJsonObject m_configuration;
    QByteArray m_bytes;
};
MetadataHttp::MetadataHttp(QObject *parent, QNetworkAccessManager *network)
    : QObject(parent), d(std::make_unique<Private>(this, network))
{
    d->m_network->setParent(this);
    d->m_clock.start();
    d->m_delay.setSingleShot(true);
    d->m_deadline.setSingleShot(true);
    connect(&d->m_delay, &QTimer::timeout, this, [this] {
        d->start();
    });
    connect(&d->m_deadline, &QTimer::timeout, this, [this] {
        d->m_error = tr("Metadata request timed out.");
        if (d->m_reply)
            d->m_reply->abort();
    });
}
MetadataHttp::~MetadataHttp()
{
    cancel();
}
void MetadataHttp::cancel()
{
    d->m_delay.stop();
    d->m_deadline.stop();
    if (auto *reply = d->m_reply.data()) {
        d->m_reply.clear();
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }
    d->m_bytes.clear();
}
void MetadataHttp::get(const QUrl &url, const QString &contact, const QJsonObject &configuration)
{
    cancel();
    d->m_url = url;
    d->m_contact = contact;
    d->m_configuration = configuration;
    d->m_error.clear();
    d->m_delay.start(0);
}
// Pace requests by service host and configure the selected proxy before creating the reply.
// The independent deadline also bounds stalls that never produce another network progress event.
void MetadataHttp::Private::start()
{
    static const QSet<QString> hosts{ u"musicbrainz.org"_s, u"zh.wikipedia.org"_s,
                                      u"en.wikipedia.org"_s, u"www.wikidata.org"_s };
    if (m_url.scheme() != "https"_L1 || !hosts.contains(m_url.host()) || !m_url.userInfo().isEmpty()
        || m_url.port(-1) != -1 || m_url.hasFragment()) {
        Q_EMIT q->completed({ { }, MetadataHttp::tr("Unsupported metadata address."), { } });
        return;
    }
    const auto key = service(m_url);
    const auto retry = m_cooldown.value(key);
    if (retry > QDateTime::currentDateTimeUtc()) {
        Q_EMIT q->completed(
                { { },
                  MetadataHttp::tr("The metadata service is cooling down. Try again later."),
                  retry.toString(Qt::ISODateWithMs) });
        return;
    }
    const auto wait = m_next.value(key, 0) - m_clock.elapsed();
    if (wait > 0) {
        m_delay.start(int(wait));
        return;
    }
    m_next.insert(key, m_clock.elapsed() + HostSpacingMilliseconds);
    const auto mode = m_configuration.value(u"proxyMode"_s).toString(u"system"_s);
    if ((m_configuration.contains(u"proxyEnabled"_s)
         && !m_configuration.value(u"proxyEnabled"_s).isBool())
        || (mode != "system"_L1 && mode != "manual"_L1)) {
        Q_EMIT q->completed({ { }, MetadataHttp::tr("Invalid HTTP proxy address."), { } });
        return;
    }
    if (!m_configuration.value(u"proxyEnabled"_s).toBool(true)) {
        m_network->setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
    } else if (mode == "system"_L1) {
        m_network->setProxy(QNetworkProxy(QNetworkProxy::DefaultProxy));
    } else {
        const QUrl proxy(m_configuration.value(u"proxyUrl"_s).toString());
        if (!metadataConfiguration::validProxy(proxy)) {
            Q_EMIT q->completed({ { }, MetadataHttp::tr("Invalid HTTP proxy address."), { } });
            return;
        }
        m_network->setProxy(QNetworkProxy(
                QNetworkProxy::HttpProxy, proxy.host(),
                quint16(proxy.port(yalahoo::metadataConfiguration::DefaultHttpProxyPort))));
    }
    QNetworkRequest request(m_url);
    QString contact = m_contact.simplified();
    contact.remove('\r'_L1);
    contact.remove('\n'_L1);
    if (contact.isEmpty())
        contact = u"https://github.com/wangwenx190/Rhine-Music-Demo"_s;
    request.setRawHeader(
            "User-Agent"_ba,
            "yalahoo/1.0 ("_ba
                    + contact.left(yalahoo::metadataConfiguration::MaximumContactCharacters)
                              .toUtf8()
                    + ")"_ba);
    request.setRawHeader("Accept"_ba, "application/json"_ba);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);
    request.setTransferTimeout(TransferTimeoutMilliseconds);
    request.setDecompressedSafetyCheckThreshold(maximumResponseBytes);
    m_reply = m_network->get(request);
    m_reply->setReadBufferSize(maximumResponseBytes + 1);
    QObject::connect(m_reply, &QNetworkReply::readyRead, q, [this] {
        read();
    });
    QObject::connect(m_reply, &QNetworkReply::finished, q, [this] {
        finish();
    });
    m_deadline.start(RequestDeadlineMilliseconds);
}
void MetadataHttp::Private::read()
{
    if (!m_reply || m_bytes.size() > maximumResponseBytes)
        return;
    m_bytes += m_reply->read(maximumResponseBytes + 1 - m_bytes.size());
    if (m_bytes.size() > maximumResponseBytes) {
        m_error = MetadataHttp::tr("Metadata response exceeds 2 MiB.");
        m_reply->abort();
    }
}
void MetadataHttp::Private::parseResponse(MetadataResponse &response)
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(m_bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        response.error = MetadataHttp::tr("The metadata service returned invalid JSON.");
        return;
    }
    response.data = document.object();
    if (!response.data.contains(u"error"_s))
        return;
    response.error = MetadataHttp::tr("The metadata service reported an API error.");
    if (response.data.value(u"error"_s).toObject().value(u"code"_s) == "maxlag"_L1) {
        const auto retry = QDateTime::currentDateTimeUtc().addSecs(FallbackRetrySeconds);
        m_cooldown.insert(service(m_url), retry);
        response.retryAt = retry.toString(Qt::ISODateWithMs);
    }
}
void MetadataHttp::Private::finish()
{
    if (!m_reply)
        return;
    m_deadline.stop();
    read();
    auto *reply = m_reply.data();
    if (!reply)
        return;
    MetadataResponse response;
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    response.error = m_error;
    if (response.error.isEmpty() && status == 0 && reply->error() != QNetworkReply::NoError)
        response.error = MetadataHttp::tr("Cannot connect to %1: %2")
                                 .arg(m_url.host(), reply->errorString());
    if (response.error.isEmpty() && (status < HttpSuccessBegin || status >= HttpSuccessEnd))
        response.error = MetadataHttp::tr("%1 returned HTTP %2.").arg(m_url.host()).arg(status);
    if (response.error.isEmpty() && reply->error() != QNetworkReply::NoError)
        response.error = MetadataHttp::tr("Cannot connect to %1: %2")
                                 .arg(m_url.host(), reply->errorString());
    const auto retryHeader = QString::fromUtf8(reply->rawHeader("Retry-After"_ba));
    if (status == HttpTooManyRequests || status == HttpUnavailable || !retryHeader.isEmpty()) {
        bool numeric = false;
        const auto seconds = retryHeader.toLongLong(&numeric);
        const auto now = QDateTime::currentDateTimeUtc();
        auto retry = numeric && seconds >= 0
                        && seconds <= MaximumRetryDays * HoursPerDay * SecondsPerHour
                ? now.addSecs(seconds)
                : QDateTime::fromString(retryHeader, Qt::RFC2822Date);
        if (!retry.isValid() || retry < now)
            retry = now.addSecs(FallbackRetrySeconds);
        // Cap service suspension to seven days and never retry automatically.
        retry = std::min(retry, now.addDays(MaximumRetryDays));
        m_cooldown.insert(service(m_url), retry);
        response.retryAt = retry.toString(Qt::ISODateWithMs);
    }
    if (response.error.isEmpty())
        parseResponse(response);
    m_reply.clear();
    reply->deleteLater();
    m_bytes.clear();
    Q_EMIT q->completed(response);
}

}
