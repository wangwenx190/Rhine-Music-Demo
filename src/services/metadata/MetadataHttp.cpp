#include "MetadataConfiguration.h"
#include <QNetworkAccessManager>
#include "MetadataHttp.h"
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
MetadataHttp::MetadataHttp(QObject *parent, QNetworkAccessManager *network)
    : QObject(parent),
      m_network(network ? network : new QNetworkAccessManager(this)),
      m_delay(this),
      m_deadline(this)
{
    m_network->setParent(this);
    m_clock.start();
    m_delay.setSingleShot(true);
    m_deadline.setSingleShot(true);
    connect(&m_delay, &QTimer::timeout, this, &MetadataHttp::start);
    connect(&m_deadline, &QTimer::timeout, this, [this] {
        m_error = tr("Metadata request timed out.");
        if (m_reply)
            m_reply->abort();
    });
}
MetadataHttp::~MetadataHttp()
{
    cancel();
}
void MetadataHttp::cancel()
{
    m_delay.stop();
    m_deadline.stop();
    if (auto *reply = m_reply.data()) {
        m_reply.clear();
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }
    m_bytes.clear();
}
void MetadataHttp::get(const QUrl &url, const QString &contact, const QJsonObject &configuration)
{
    cancel();
    m_url = url;
    m_contact = contact;
    m_configuration = configuration;
    m_error.clear();
    m_delay.start(0);
}
// Pace requests by service host and configure the selected proxy before creating the reply.
// The independent deadline also bounds stalls that never produce another network progress event.
void MetadataHttp::start()
{
    static const QSet<QString> hosts{ u"musicbrainz.org"_s, u"zh.wikipedia.org"_s,
                                      u"en.wikipedia.org"_s, u"www.wikidata.org"_s };
    if (m_url.scheme() != "https"_L1 || !hosts.contains(m_url.host()) || !m_url.userInfo().isEmpty()
        || m_url.port(-1) != -1 || m_url.hasFragment()) {
        Q_EMIT completed({ { }, tr("Unsupported metadata address."), { } });
        return;
    }
    const auto key = service(m_url);
    const auto retry = m_cooldown.value(key);
    if (retry > QDateTime::currentDateTimeUtc()) {
        Q_EMIT completed({ { },
                           tr("The metadata service is cooling down. Try again later."),
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
        Q_EMIT completed({ { }, tr("Invalid HTTP proxy address."), { } });
        return;
    }
    if (!m_configuration.value(u"proxyEnabled"_s).toBool(true)) {
        m_network->setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
    } else if (m_configuration.value(u"proxyMode"_s).toString(u"system"_s) == "system"_L1) {
        m_network->setProxy(QNetworkProxy(QNetworkProxy::DefaultProxy));
    } else {
        const QUrl proxy(m_configuration.value(u"proxyUrl"_s).toString());
        if (!metadataConfiguration::validProxy(proxy)) {
            Q_EMIT completed({ { }, tr("Invalid HTTP proxy address."), { } });
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
    connect(m_reply, &QNetworkReply::readyRead, this, &MetadataHttp::read);
    connect(m_reply, &QNetworkReply::finished, this, &MetadataHttp::finish);
    m_deadline.start(RequestDeadlineMilliseconds);
}
void MetadataHttp::read()
{
    if (!m_reply || m_bytes.size() > maximumResponseBytes)
        return;
    m_bytes += m_reply->read(maximumResponseBytes + 1 - m_bytes.size());
    if (m_bytes.size() > maximumResponseBytes) {
        m_error = tr("Metadata response exceeds 2 MiB.");
        m_reply->abort();
    }
}
void MetadataHttp::parseResponse(MetadataResponse &response)
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(m_bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        response.error = tr("The metadata service returned invalid JSON.");
        return;
    }
    response.data = document.object();
    if (!response.data.contains(u"error"_s))
        return;
    response.error = tr("The metadata service reported an API error.");
    if (response.data.value(u"error"_s).toObject().value(u"code"_s) == "maxlag"_L1) {
        const auto retry = QDateTime::currentDateTimeUtc().addSecs(FallbackRetrySeconds);
        m_cooldown.insert(service(m_url), retry);
        response.retryAt = retry.toString(Qt::ISODateWithMs);
    }
}
void MetadataHttp::finish()
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
        response.error = tr("Cannot connect to %1: %2").arg(m_url.host(), reply->errorString());
    if (response.error.isEmpty() && (status < HttpSuccessBegin || status >= HttpSuccessEnd))
        response.error = tr("%1 returned HTTP %2.").arg(m_url.host()).arg(status);
    if (response.error.isEmpty() && reply->error() != QNetworkReply::NoError)
        response.error = tr("Cannot connect to %1: %2").arg(m_url.host(), reply->errorString());
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
    Q_EMIT completed(response);
}
}
