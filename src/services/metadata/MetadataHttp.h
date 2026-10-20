#pragma once
#include <QElapsedTimer>
#include <QUrl>
#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QPointer>
#include <QTimer>

class QNetworkAccessManager;
class QNetworkReply;

namespace yalahoo {
struct MetadataResponse final
{
    QJsonObject data;
    QString error, retryAt;
};
// One bounded request, owned and used entirely by the metadata thread.
class MetadataHttp final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MetadataHttp)
public:
    explicit MetadataHttp(QObject *parent = nullptr, QNetworkAccessManager *network = nullptr);
    ~MetadataHttp() override;
    void get(const QUrl &url, const QString &contact, const QJsonObject &configuration);
    void cancel();
Q_SIGNALS:
    void completed(const yalahoo::MetadataResponse &response);

private:
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
}
