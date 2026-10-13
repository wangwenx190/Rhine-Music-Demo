#pragma once
#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <atomic>
#include <memory>

namespace yalahoo {
// Values published once by the worker; consumers only share immutable, owned pixels.
struct CoverPixels
{
    QString identity, path, error;
    quint64 generation = 0;
    int resolution = 256;
    QSize sourceSize;
    bool decoderScaling = false;
    qint64 readerReturnedBytes = 0;
    QImage image;
};
using CoverSnapshot = std::shared_ptr<const CoverPixels>;

class CoverCache : public QObject
{
    Q_OBJECT
public:
    explicit CoverCache(QObject *parent = nullptr);
    ~CoverCache() override;
    bool request(QString identity, QString path, int resolution, QString title = { });
    CoverSnapshot lookup(const QString &identity, const QString &path, int resolution);
    void reset();
    void cancel(const QString &identity);
    quint64 generation() const;
    QJsonObject statistics() const;
    static QImage paint(const QImage &source, int resolution, const QString &title);
    static constexpr int concurrency = 3;
    static constexpr int pendingLimit = 96;
    static constexpr qint64 cacheLimit = 16 * 1024 * 1024;
    // Conservative source expansion incl. high precision decoders, plus conversion/paint scratch.
    static constexpr qint64 sourcePixelLimit = 4 * 1024 * 1024;
    static constexpr qint64 workerReservation = sourcePixelLimit * 32 + 12 * 1024 * 1024;

signals:
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
    static QString key(const QString &identity, const QString &path, int resolution);
    void pump();
    void complete(const Job &job, CoverSnapshot pixels);
    std::shared_ptr<State> m_state;
    QTimer m_retry;
    QList<Job> m_pending;
    QHash<QString, Job> m_running;
    QHash<QString, Entry> m_cache;
    qint64 m_bytes = 0;
    int m_active = 0, m_peakActive = 0, m_peakPending = 0;
    quint64 m_clock = 0, m_decodes = 0, m_rejected = 0, m_stale = 0;
    qint64 m_peakReaderBytes = 0;
    int m_decoderScaledResults = 0;
};
}
Q_DECLARE_METATYPE(yalahoo::CoverSnapshot)
