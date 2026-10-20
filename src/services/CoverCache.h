#pragma once
#include <QHash>
#include <QImage>
#include <QList>
#include <QPoint>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QSet>
#include <atomic>
#include <memory>

namespace yalahoo {
// Values published once by the worker; consumers only share immutable, owned pixels.
struct CoverMipTile
{
    QPoint origin;
    QImage image;
    QList<QImage> mipmaps;
};
struct CoverPixels
{
    QString identity, path, error, fallbackTitle;
    quint64 generation = 0;
    int resolution = 256;
    QSize sourceSize;
    bool decoderScaling = false;
    qint64 readerReturnedBytes = 0;
    QImage image;
    QList<QImage> mipmaps;
    QList<CoverMipTile> tiles;
    qint64 byteSize() const;
};
using CoverSnapshot = std::shared_ptr<const CoverPixels>;

class CoverCache : public QObject
{
    Q_OBJECT
public:
    explicit CoverCache(QObject *parent = nullptr);
    ~CoverCache() override;
    // Final process teardown only, while QGuiApplication/font services still exist.
    static void finishDecoders();
    bool request(QString identity, QString path, int resolution, QString title = { });
    CoverSnapshot lookup(const QString &identity, const QString &path, int resolution,
                         const QString &title = { });
    void reset();
    void setSuspended(bool suspended);
    void cancel(const QString &identity);
    void invalidate(const QString &identity);
    quint64 generation() const;
    QJsonObject statistics() const;
    bool hasPendingCapacity() const;
    static QList<QImage> makeMipmaps(const QImage &image);
    static QImage paint(const QImage &source, int resolution, const QString &title);
    static constexpr int concurrency = 3;
    static constexpr int pendingLimit = 96;
    static constexpr qint64 cacheLimit = 16 * 1024 * 1024;
    static qint64 workingSetEstimateFor(int resolution);

Q_SIGNALS:
    void ready(yalahoo::CoverSnapshot pixels);

private:
    struct Job
    {
        QString key, identity, path, title;
        int resolution = 256;
        quint64 generation = 0;
        std::shared_ptr<std::atomic_bool> wanted;
    };
    struct State;
    struct Entry
    {
        CoverSnapshot pixels;
        quint64 used = 0;
    };
    static CoverSnapshot decode(const Job &job, const std::shared_ptr<State> &state);
    static QString key(const QString &identity, const QString &path, int resolution,
                       const QString &title);
    void pump();
    void complete(const Job &job, CoverSnapshot pixels);
    std::shared_ptr<State> m_state;
    QTimer m_retry;
    QList<Job> m_pending;
    QSet<QString> m_pendingKeys;
    QHash<QString, Job> m_running;
    QHash<QString, Entry> m_cache;
    qint64 m_bytes = 0, m_workingSetBytes = 0, m_peakWorkingSetBytes = 0;
    bool m_suspended = false;
    int m_active = 0, m_peakActive = 0, m_peakPending = 0;
    quint64 m_clock = 0, m_decodes = 0, m_rejected = 0, m_stale = 0;
    qint64 m_peakReaderBytes = 0;
    int m_decoderScaledResults = 0;
};
}
Q_DECLARE_METATYPE(yalahoo::CoverSnapshot)
