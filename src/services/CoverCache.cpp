#include <QCoreApplication>
#include "CoverCache.h"
#include "MusicTypes.h"
#include <QImageReader>
#include <QColorSpace>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QFontMetricsF>
#include <QPointer>
#include <QThreadPool>
#include <stdexcept>

using namespace Qt::StringLiterals;

namespace yalahoo {
struct CoverCache::State
{
    QMutex mutex;
    CoverCache *receiver = nullptr;
    std::atomic<quint64> generation{ 1 };
};
namespace {
QThreadPool &decodePool()
{
    static QThreadPool pool;
    static const bool configured = [] {
        pool.setMaxThreadCount(CoverCache::concurrency);
        return true;
    }();
    Q_UNUSED(configured);
    return pool;
}
}
CoverCache::CoverCache(QObject *parent) : QObject(parent), m_state(std::make_shared<State>())
{
    m_state->receiver = this;
    m_retry.setSingleShot(true);
    m_retry.setInterval(10);
    connect(&m_retry, &QTimer::timeout, this, &CoverCache::pump);
}
CoverCache::~CoverCache()
{
    // The lock only covers posting callbacks, never decoding. No worker wait on GUI shutdown.
    const QMutexLocker lock(&m_state->mutex);
    m_state->receiver = nullptr;
    ++m_state->generation;
}
void CoverCache::finishDecoders()
{
    decodePool().waitForDone();
}
QString CoverCache::key(const QString &identity, const QString &path, int resolution,
                        const QString &title)
{
    // Missing-image pixels depend on title, including after an identity leaves the visible pool.
    const auto &fallback = title;
    // Length prefixes avoid ambiguous identities/paths containing separators.
    return QString::number(identity.size()) + ':'_L1 + identity + QString::number(path.size())
            + ':'_L1 + path + ':'_L1 + QString::number(resolution) + ':'_L1
            + QString::number(fallback.size()) + ':'_L1 + fallback;
}
quint64 CoverCache::generation() const
{
    return m_state->generation.load();
}
CoverSnapshot CoverCache::lookup(const QString &identity, const QString &path, int resolution,
                                 const QString &title)
{
    if (!path.isEmpty()) {
        auto artwork = m_cache.find(key(identity, path, resolution, { }));
        if (artwork != m_cache.end() && artwork->pixels->error.isEmpty()) {
            artwork->used = ++m_clock;
            return artwork->pixels;
        }
    }
    auto it = m_cache.find(key(identity, path, resolution, title));
    if (it == m_cache.end())
        return { };
    it->used = ++m_clock;
    return it->pixels;
}
bool CoverCache::request(QString identity, QString path, int resolution, QString title)
{
    if (identity.isEmpty() || identity.size() > 1024 || path.size() > 32768 || title.size() > 256
        || (resolution != 256 && resolution != 1024)) {
        ++m_rejected;
        return false;
    }
    const auto cacheKey = key(identity, path, resolution, title);
    if (lookup(identity, path, resolution, title))
        return true;
    const auto running = m_running.constFind(cacheKey);
    if (running != m_running.cend() && running->generation == generation()
        && running->wanted->load())
        return true;
    if (m_pendingKeys.contains(cacheKey))
        return true;
    if (m_pending.size() >= pendingLimit) {
        ++m_rejected;
        return false;
    }
    m_pending.append({ cacheKey, std::move(identity), std::move(path), std::move(title), resolution,
                       generation(), std::make_shared<std::atomic_bool>(true) });
    m_pendingKeys.insert(cacheKey);
    m_peakPending = qMax(m_peakPending, int(m_pending.size()));
    pump();
    return true;
}
void CoverCache::reset()
{
    ++m_state->generation;
    m_pending.clear();
    m_pendingKeys.clear();
    m_cache.clear();
    m_bytes = 0;
    // Running jobs retain their reservations until their completion is consumed.
    m_retry.stop();
}
void CoverCache::cancel(const QString &identity)
{
    m_pending.removeIf([this, &identity](const Job &job) {
        if (job.identity == identity)
            m_pendingKeys.remove(job.key);
        return job.identity == identity;
    });
    for (const auto &job : m_running) {
        if (job.identity == identity)
            job.wanted->store(false);
    }
}
void CoverCache::invalidate(const QString &identity)
{
    cancel(identity);
    for (auto it = m_cache.begin(); it != m_cache.end();) {
        if (it->pixels->identity == identity) {
            m_bytes -= it->pixels->image.sizeInBytes();
            it = m_cache.erase(it);
        } else {
            ++it;
        }
    }
}
void CoverCache::setSuspended(bool suspended)
{
    if (m_suspended == suspended)
        return;
    m_suspended = suspended;
    if (suspended)
        m_retry.stop();
    else
        pump();
}
void CoverCache::pump()
{
    if (m_suspended)
        return;
    while (m_active < concurrency && !m_pending.isEmpty()) {
        const auto job = m_pending.first();
        const auto state = m_state;
        const bool started = decodePool().tryStart([job, state] {
            const auto pixels = decode(job, state);
            const QMutexLocker lock(&state->mutex);
            if (state->receiver) {
                auto *receiver = state->receiver;
                QMetaObject::invokeMethod(
                        receiver,
                        [receiver, job, pixels] {
                            receiver->complete(job, pixels);
                        },
                        Qt::QueuedConnection);
            }
        });
        if (!started) {
            m_retry.start();
            return;
        }
        m_pending.removeFirst();
        m_pendingKeys.remove(job.key);
        m_running.insert(job.key, job);
        ++m_active;
        ++m_decodes;
        m_peakActive = qMax(m_peakActive, m_active);
    }
}
QImage CoverCache::paint(const QImage &source, int resolution, const QString &title)
{
    if (resolution != 256 && resolution != 1024)
        throw std::invalid_argument("Invalid cover paint resolution");
    QImage image(resolution, resolution, QImage::Format_RGBA8888_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const qreal margin = resolution / 64.0;
    const QRectF box(margin, margin, resolution - 2 * margin, resolution - 2 * margin);
    if (!source.isNull()) {
        const qreal scale = qMin(box.width() / source.width(), box.height() / source.height());
        const QSizeF size(source.width() * scale, source.height() * scale);
        painter.drawImage(QRectF(box.center() - QPointF(size.width() / 2, size.height() / 2), size),
                          source);
    } else {
        // The source paints fallback text in a 1024 logical canvas at both resolutions.
        constexpr qreal logical = 1024;
        painter.scale(resolution / logical, resolution / logical);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(QRectF(16, 16, 992, 992), QColor("#c9c9c4"_L1));
        painter.setPen(QPen(QColor("#f8f7f1"_L1), logical / 180));
        painter.drawEllipse(QPointF(logical * .5, logical * .43), logical * .2, logical * .2);
        painter.drawEllipse(QPointF(logical * .5, logical * .43), logical * .04, logical * .04);
        painter.setPen(QColor("#3f4849"_L1));
        QFont font(u"sans-serif"_s);
        font.setWeight(QFont::Medium);
        auto label = [&](const QString &text, qreal pixels, qreal baseline) {
            font.setPointSizeF(pixels * 72 / painter.device()->logicalDpiY());
            painter.setFont(font);
            const qreal width = QFontMetricsF(font, painter.device()).horizontalAdvance(text);
            painter.save();
            painter.translate(logical / 2, baseline);
            painter.scale(qMin(1.0, logical * .83 / qMax(1.0, width)), 1);
            painter.drawText(QPointF(-width / 2, 0), text);
            painter.restore();
        };
        label(title.isEmpty() ? QCoreApplication::translate("Yalahoo", "No album cover")
                              : graphemePrefix(title, 256),
              logical * .045, logical * .8);
        font.setWeight(QFont::Normal);
        label(u"LOCAL COLLECTION / NO COVER"_s, logical * .025, logical * .87);
    }
    return image;
}
CoverSnapshot CoverCache::decode(const Job &job, const std::shared_ptr<State> &state)
{
    if (job.generation != state->generation.load() || !job.wanted->load())
        return { };
    auto pixels = std::make_shared<CoverPixels>();
    pixels->identity = job.identity;
    pixels->path = job.path;
    pixels->generation = job.generation;
    pixels->resolution = job.resolution;
    QImage source;
    // A known absent cover goes straight to the authored placeholder, without opening an empty URL.
    if (job.path.isEmpty()) {
        pixels->error = u"Missing cover"_s;
    } else {
        QImageReader reader(job.path);
        reader.setAutoTransform(true);
        pixels->sourceSize = reader.size();
        pixels->decoderScaling = reader.supportsOption(QImageIOHandler::ScaledSize);
        const auto size = pixels->sourceSize;
        // Source size is checked even for scaling plugins: their internal scratch is not observable.
        if (!size.isValid() || qint64(size.width()) * size.height() > sourcePixelLimit) {
            pixels->error = u"Missing/unknown cover dimensions or source exceeds 4M pixels"_s;
        } else if (job.generation == state->generation.load() && job.wanted->load()) {
            if (qMax(size.width(), size.height()) > job.resolution)
                reader.setScaledSize(
                        size.scaled(job.resolution, job.resolution, Qt::KeepAspectRatio));
            source = reader.read();
            pixels->readerReturnedBytes = source.sizeInBytes();
            if (source.isNull()) {
                pixels->error = reader.errorString();
                if (pixels->error.isEmpty())
                    pixels->error = u"Cover decoder returned no image"_s;
            }
        }
    }
    if (job.generation != state->generation.load() || !job.wanted->load())
        return { };
    if (!source.isNull() && source.colorSpace().isValid())
        source.convertToColorSpace(QColorSpace::SRgb);
    if (!pixels->error.isEmpty())
        pixels->fallbackTitle = job.title;
    pixels->image = paint(source, job.resolution, job.title);
    if (job.generation != state->generation.load() || !job.wanted->load())
        return { };
    return pixels;
}
void CoverCache::complete(const Job &job, CoverSnapshot pixels)
{
    --m_active;
    if (m_running.value(job.key).wanted == job.wanted)
        m_running.remove(job.key);
    if (job.generation != generation() || !job.wanted->load() || !pixels) {
        ++m_stale;
    } else {
        const qint64 bytes = pixels->image.sizeInBytes();
        const auto completedKey = pixels->error.isEmpty() && !job.path.isEmpty()
                ? key(job.identity, job.path, job.resolution, { })
                : job.key;
        if (const auto previous = m_cache.find(completedKey); previous != m_cache.end()) {
            m_bytes -= previous->pixels->image.sizeInBytes();
            m_cache.erase(previous);
        }
        m_peakReaderBytes = qMax(m_peakReaderBytes, pixels->readerReturnedBytes);
        m_decoderScaledResults += pixels->decoderScaling && pixels->error.isEmpty();
        while (m_bytes + bytes > cacheLimit && !m_cache.isEmpty()) {
            auto oldest = m_cache.begin();
            for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
                if (it->used < oldest->used)
                    oldest = it;
            }
            m_bytes -= oldest->pixels->image.sizeInBytes();
            m_cache.erase(oldest);
        }
        m_cache.insert(completedKey, { pixels, ++m_clock });
        m_bytes += bytes;
        const QPointer<CoverCache> guard(this);
        Q_EMIT ready(pixels);
        if (!guard)
            return;
    }
    pump();
}
QJsonObject CoverCache::statistics() const
{
    return { { u"generation"_s, qint64(generation()) },
             { u"suspended"_s, m_suspended },
             { u"pending"_s, m_pending.size() },
             { u"active"_s, m_active },
             { u"peakPending"_s, m_peakPending },
             { u"peakActive"_s, m_peakActive },
             { u"peakReservedInFlightBytes"_s, m_peakActive * workerReservation },
             { u"peakReaderReturnedBytesPerJob"_s, m_peakReaderBytes },
             { u"decoderScaledResults"_s, m_decoderScaledResults },
             { u"completedLruBytes"_s, m_bytes },
             { u"reservedInFlightBytes"_s, m_active * workerReservation },
             { u"decodeStarts"_s, qint64(m_decodes) },
             { u"rejected"_s, qint64(m_rejected) },
             { u"stale"_s, qint64(m_stale) } };
}
bool CoverCache::hasPendingCapacity() const
{
    return m_pending.size() < pendingLimit;
}
}
