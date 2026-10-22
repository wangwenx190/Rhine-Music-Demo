// Serializes one metadata lookup through HTTP source stages and bounds waiting jobs.
// The provider moves this object and all of its QObject children to its dedicated thread.

#pragma once
#include "IMetadataProvider.h"
#include "MetadataHttp.h"
#include <QJsonArray>
#include <optional>

namespace yalahoo {
using Qt::StringLiterals::operator""_s;
class MetadataWorker final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MetadataWorker)
public:
    explicit MetadataWorker(QNetworkAccessManager *network);
    void enqueue(AlbumQuery query, RequestToken token, MetadataKind kind);
    void cancel(const RequestToken &token);
    void stop();
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
        QString releaseId, groupId, inputKey;
    };
    void start();

    void get(const QUrl &url);
    void release(const QString &id);
    void pages(bool search);
    void entities(const QStringList &ids, Stage stage);
    void assess();
    void receive(const MetadataResponse &response);
    void receiveSearch(const QJsonObject &data);
    void receiveRelease(const QJsonObject &data);
    void receiveGroup(const QJsonObject &data);
    void receivePages(const QJsonObject &data);
    void receiveEntities(const QJsonObject &data);

    void fail(const QString &message);
    void finish(bool checked = true);
    MetadataHttp m_http;
    QTimer m_next;
    QList<Job> m_waiting;
    std::optional<Job> m_active;
};
}
