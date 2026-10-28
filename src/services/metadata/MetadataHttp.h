// Serializes provider HTTP work with bounded replies and cancellation. Host pacing, proxy
// configuration and request identity are enforced on the network thread.

#pragma once
#include <QObject>
#include <QJsonObject>
#include <QUrl>
#include <memory>

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
    struct Private;
    std::unique_ptr<Private> d;
};
}
