// Serializes one metadata lookup through HTTP source stages and bounds waiting jobs.
// The provider moves this object and all of its QObject children to its dedicated thread.

#pragma once
#include "IMetadataProvider.h"
#include <memory>

class QNetworkAccessManager;
namespace yalahoo {
struct MetadataResponse;
}

namespace yalahoo {
using Qt::StringLiterals::operator""_s;
class MetadataWorker final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MetadataWorker)
public:
    explicit MetadataWorker(QNetworkAccessManager *network);
    ~MetadataWorker() override;
    void enqueue(AlbumQuery query, RequestToken token, MetadataKind kind);
    void cancel(const RequestToken &token);
    void stop();
Q_SIGNALS:
    void ready(const yalahoo::ProviderResult &result);

private:
    struct Private;
    std::unique_ptr<Private> d;
};
}
