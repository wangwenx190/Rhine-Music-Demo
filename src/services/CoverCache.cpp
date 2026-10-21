#include "ColorTransfer.h"
#include "Units.h"
#include <atomic>
#include <QCoreApplication>
#include "CoverCache.h"
#include <QSet>
#include <QTimer>
#include <QHash>
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
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
// Cover geometry uses a 1024-unit fallback canvas; color conversion follows the sRGB transfer curve.
constexpr auto ByteChannelValues = 256;
constexpr auto MaximumFallbackTitleCharacters = 256;
constexpr auto FallbackCanvasUnits = 1024;
constexpr auto DecodeRetryMilliseconds = 10;
constexpr auto MaximumIdentityCharacters = 1024;
constexpr auto BytesPerMiB = 1024 * 1024;
constexpr auto MaximumPathCharacters = 32768;
constexpr auto WorkingBytesPerOutputPixel = 36;
constexpr auto FallbackMarginDivisor = 64.0;
constexpr auto FallbackInsetPixels = 16;
constexpr auto FallbackInnerPixels = 992;
constexpr auto FallbackStrokeDivisor = 180;
constexpr auto FallbackDiscCenterY = .43;
constexpr auto FallbackDiscRadius = .2;
constexpr auto FallbackSpindleRadius = .04;
constexpr auto PointsPerInch = 72;
constexpr auto FallbackMaximumLabelWidth = .83;
constexpr auto FallbackTitleSizeRatio = .045;
constexpr auto FallbackTitleY = .8;
constexpr auto FallbackCaptionSize = .025;
constexpr auto FallbackCaptionY = .87;
constexpr auto ByteChannelScale = 255.0;
constexpr auto FloatByteChannelScale = 255.0f;
constexpr auto TileMipLevels = 8;

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
qint64 CoverPixels::byteSize() const
{
    qint64 bytes = 0;
    QSet<qint64> seen;
    const auto count = [&](const QImage &pixels) {
        if (!pixels.isNull() && !seen.contains(pixels.cacheKey())) {
            seen.insert(pixels.cacheKey());
            bytes += pixels.sizeInBytes();
        }
    };
    count(image);
    for (const auto &mip : mipmaps)
        count(mip);
    for (const auto &tile : tiles) {
        count(tile.image);
        for (const auto &mip : tile.mipmaps)
            count(mip);
    }
    return bytes;
}
struct CoverCache::Job final
{
    QString key, identity, path, title;
    int resolution = yalahoo::coverPixels::TileSide;
    quint64 generation = 0;
    std::shared_ptr<std::atomic_bool> wanted;
};
struct CoverCache::Entry final
{
    CoverSnapshot pixels;
    quint64 used = 0;
};
struct CoverCache::State final
{
    Q_DISABLE_COPY_MOVE(State)
    State() = default;
    QMutex mutex;
    CoverCache *receiver = nullptr;
    std::atomic<quint64> generation{ 1 };
};
struct CoverCache::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    Private() = default;
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
CoverCache::CoverCache(QObject *parent) : QObject(parent), d(std::make_unique<Private>())
{
    d->m_state = std::make_shared<State>();
    d->m_state->receiver = this;
    d->m_retry.setSingleShot(true);
    d->m_retry.setInterval(DecodeRetryMilliseconds);
    connect(&d->m_retry, &QTimer::timeout, this, &CoverCache::pump);
}
CoverCache::~CoverCache()
{
    // The lock only covers posting callbacks, never decoding. No worker wait on GUI shutdown.
    const QMutexLocker lock(&d->m_state->mutex);
    d->m_state->receiver = nullptr;
    ++d->m_state->generation;
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
    return d->m_state->generation.load();
}
CoverSnapshot CoverCache::lookup(const QString &identity, const QString &path, int resolution,
                                 const QString &title)
{
    if (!path.isEmpty()) {
        auto artwork = d->m_cache.find(key(identity, path, resolution, { }));
        if (artwork != d->m_cache.end() && artwork->pixels->error.isEmpty()) {
            artwork->used = ++d->m_clock;
            return artwork->pixels;
        }
    }
    auto it = d->m_cache.find(key(identity, path, resolution, title));
    if (it == d->m_cache.end())
        return { };
    it->used = ++d->m_clock;
    return it->pixels;
}
bool CoverCache::request(QString identity, QString path, int resolution, QString title)
{
    if (identity.isEmpty() || identity.size() > MaximumIdentityCharacters
        || path.size() > MaximumPathCharacters || title.size() > MaximumFallbackTitleCharacters
        || (resolution <= 0 || (resolution & (resolution - 1)) != 0)
        || qint64(resolution)
                > std::numeric_limits<qint64>::max() / resolution / WorkingBytesPerOutputPixel) {
        ++d->m_rejected;
        return false;
    }
    const auto cacheKey = key(identity, path, resolution, title);
    if (lookup(identity, path, resolution, title))
        return true;
    const auto running = d->m_running.constFind(cacheKey);
    if (running != d->m_running.cend() && running->generation == generation()
        && running->wanted->load())
        return true;
    if (d->m_pendingKeys.contains(cacheKey))
        return true;
    if (d->m_pending.size() >= pendingLimit) {
        ++d->m_rejected;
        return false;
    }
    d->m_pending.append({ cacheKey, std::move(identity), std::move(path), std::move(title),
                          resolution, generation(), std::make_shared<std::atomic_bool>(true) });
    d->m_pendingKeys.insert(cacheKey);
    d->m_peakPending = qMax(d->m_peakPending, int(d->m_pending.size()));
    pump();
    return true;
}
void CoverCache::reset()
{
    ++d->m_state->generation;
    d->m_pending.clear();
    d->m_pendingKeys.clear();
    d->m_cache.clear();
    d->m_bytes = 0;
    // Running jobs retain their reservations until their completion is consumed.
    d->m_retry.stop();
}
void CoverCache::cancel(const QString &identity)
{
    d->m_pending.removeIf([this, &identity](const Job &job) {
        if (job.identity == identity)
            d->m_pendingKeys.remove(job.key);
        return job.identity == identity;
    });
    for (const auto &job : d->m_running) {
        if (job.identity == identity)
            job.wanted->store(false);
    }
}
void CoverCache::invalidate(const QString &identity)
{
    cancel(identity);
    for (auto it = d->m_cache.begin(); it != d->m_cache.end();) {
        if (it->pixels->identity == identity) {
            d->m_bytes -= it->pixels->byteSize();
            it = d->m_cache.erase(it);
        } else {
            ++it;
        }
    }
}
void CoverCache::setSuspended(bool suspended)
{
    if (d->m_suspended == suspended)
        return;
    d->m_suspended = suspended;
    if (suspended)
        d->m_retry.stop();
    else
        pump();
}
void CoverCache::decodeAndDeliver(const Job &job, const std::shared_ptr<State> &state)
{
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
}
void CoverCache::pump()
{
    if (d->m_suspended)
        return;
    while (d->m_active < concurrency && !d->m_pending.isEmpty()) {
        const auto job = d->m_pending.first();
        const auto state = d->m_state;
        const bool started = decodePool().tryStart([job, state] {
            decodeAndDeliver(job, state);
        });
        if (!started) {
            d->m_retry.start();
            return;
        }
        d->m_pending.removeFirst();
        d->m_pendingKeys.remove(job.key);
        d->m_running.insert(job.key, job);
        ++d->m_active;
        d->m_workingSetBytes += workingSetEstimateFor(job.resolution);
        d->m_peakWorkingSetBytes = std::max(d->m_peakWorkingSetBytes, d->m_workingSetBytes);
        ++d->m_decodes;
        d->m_peakActive = qMax(d->m_peakActive, d->m_active);
    }
}
qint64 CoverCache::workingSetEstimateFor(int resolution)
{
    // Requested paint/mip working storage only. Source decoder and color-conversion
    // allocations are independent and are not a process-memory reservation.
    return qint64(resolution) * resolution * WorkingBytesPerOutputPixel;
}
QImage CoverCache::paint(const QImage &source, int resolution, const QString &title)
{
    if (resolution <= 0 || (resolution & (resolution - 1)) != 0)
        throw std::invalid_argument("Invalid cover paint resolution");
    QImage image(resolution, resolution, QImage::Format_RGBA8888_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const qreal margin = resolution / FallbackMarginDivisor;
    const QRectF box(margin, margin, resolution - 2 * margin, resolution - 2 * margin);
    if (!source.isNull()) {
        const qreal scale = qMin(box.width() / source.width(), box.height() / source.height());
        const QSizeF size(source.width() * scale, source.height() * scale);
        painter.drawImage(QRectF(box.center() - QPointF(size.width() / 2, size.height() / 2), size),
                          source);
    } else {
        // The source paints fallback text in a 1024 logical canvas at both resolutions.
        constexpr qreal logical = FallbackCanvasUnits;
        painter.scale(resolution / logical, resolution / logical);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(QRectF(FallbackInsetPixels, FallbackInsetPixels, FallbackInnerPixels,
                                FallbackInnerPixels),
                         QColor("#c9c9c4"_L1));
        painter.setPen(QPen(QColor("#f8f7f1"_L1), logical / FallbackStrokeDivisor));
        painter.drawEllipse(QPointF(logical * .5, logical * FallbackDiscCenterY),
                            logical * FallbackDiscRadius, logical * FallbackDiscRadius);
        painter.drawEllipse(QPointF(logical * .5, logical * FallbackDiscCenterY),
                            logical * FallbackSpindleRadius, logical * FallbackSpindleRadius);
        painter.setPen(QColor("#3f4849"_L1));
        QFont font(u"sans-serif"_s);
        font.setWeight(QFont::Medium);
        auto label = [&](const QString &text, qreal pixels, qreal baseline) {
            font.setPointSizeF(pixels * PointsPerInch / painter.device()->logicalDpiY());
            painter.setFont(font);
            const qreal width = QFontMetricsF(font, painter.device()).horizontalAdvance(text);
            painter.save();
            painter.translate(logical / 2, baseline);
            painter.scale(qMin(1.0, logical * FallbackMaximumLabelWidth / qMax(1.0, width)), 1);
            painter.drawText(QPointF(-width / 2, 0), text);
            painter.restore();
        };
        label(title.isEmpty() ? QCoreApplication::translate("Yalahoo", "No album cover")
                              : graphemePrefix(title, MaximumFallbackTitleCharacters),
              logical * FallbackTitleSizeRatio, logical * FallbackTitleY);
        font.setWeight(QFont::Normal);
        label(u"LOCAL COLLECTION / NO COVER"_s, logical * FallbackCaptionSize,
              logical * FallbackCaptionY);
    }
    return image;
}
// Filter linear-light premultiplied color and alpha together before returning to sRGB.
// Each destination pixel integrates its source footprint, avoiding dark fringes at transparent edges.
QList<QImage> CoverCache::makeMipmaps(const QImage &image)
{
    // Decode sRGB before area reduction; alpha remains premultiplied in linear light.
    // Independent chains never incorporate pixels belonging to another atlas slot.
    const auto source = image.convertToFormat(QImage::Format_RGBA8888);
    const auto decode = [](double v) {
        return colorTransfer::decode(v);
    };
    const auto encode = [](double v) {
        return colorTransfer::encode(v);
    };
    std::array<float, ByteChannelValues> decoded;
    for (int i = 0; i < ByteChannelValues; ++i)
        decoded[i] = float(decode(i / ByteChannelScale));
    QImage linear(source.size(), QImage::Format_RGBA32FPx4_Premultiplied);
    for (int y = 0; y < source.height(); ++y) {
        const auto *input = source.constScanLine(y);
        auto *row = reinterpret_cast<float *>(linear.scanLine(y));
        for (int x = 0; x < source.width(); ++x) {
            const float alpha = input[4 * x + 3] / FloatByteChannelScale;
            for (int c = 0; c < 3; ++c)
                row[4 * x + c] = decoded[input[4 * x + c]] * alpha;
            row[4 * x + 3] = alpha;
        }
    }
    QList<QImage> levels;
    while (linear.width() > 1 || linear.height() > 1) {
        const QSize size(qMax(1, linear.width() / 2), qMax(1, linear.height() / 2));
        QImage next(size, linear.format());
        QImage encoded(size, QImage::Format_RGBA8888_Premultiplied);
        encoded.setColorSpace(QColorSpace::SRgb);
        const double sx = double(linear.width()) / size.width();
        const double sy = double(linear.height()) / size.height();
        for (int y = 0; y < size.height(); ++y) {
            auto *output = reinterpret_cast<float *>(next.scanLine(y));
            auto *bytes = encoded.scanLine(y);
            for (int x = 0; x < size.width(); ++x) {
                std::array<double, 4> sum{ };
                const double x0 = x * sx, x1 = (x + 1) * sx;
                const double y0 = y * sy, y1 = (y + 1) * sy;
                for (int iy = int(y0); iy < int(std::ceil(y1)); ++iy) {
                    const auto *row = reinterpret_cast<const float *>(linear.constScanLine(iy));
                    for (int ix = int(x0); ix < int(std::ceil(x1)); ++ix) {
                        const double weight = (std::min(x1, ix + 1.0) - std::max(x0, double(ix)))
                                * (std::min(y1, iy + 1.0) - std::max(y0, double(iy)));
                        for (int c = 0; c < 4; ++c)
                            sum[c] += row[4 * ix + c] * weight;
                    }
                }
                for (int c = 0; c < 4; ++c)
                    output[4 * x + c] = float(sum[c] / (sx * sy));
                const double alpha = output[4 * x + 3];
                for (int c = 0; c < 3; ++c)
                    bytes[4 * x + c] = uchar(
                            std::clamp(qRound(alpha > 0 ? encode(output[4 * x + c] / alpha) * alpha
                                                              * yalahoo::units::MaximumChannelByte
                                                        : 0),
                                       0, yalahoo::units::MaximumChannelByte));
                bytes[4 * x + 3] =
                        uchar(std::clamp(qRound(alpha * yalahoo::units::MaximumChannelByte), 0,
                                         yalahoo::units::MaximumChannelByte));
            }
        }
        levels.append(std::move(encoded));
        linear = std::move(next);
    }
    return levels;
}
// Decode into worker-owned pixels, then publish an immutable snapshot.
// Cancellation/generation checks protect the facade; decoded byte limits protect allocation.
QImage CoverCache::readSource(const Job &job, const std::shared_ptr<State> &state,
                              CoverPixels &pixels)
{
    if (job.path.isEmpty()) {
        pixels.error = u"Missing cover"_s;
        return { };
    }
    QImageReader reader(job.path);
    reader.setAutoTransform(true);
    pixels.sourceSize = reader.size();
    pixels.decoderScaling = reader.supportsOption(QImageIOHandler::ScaledSize);
    const auto size = pixels.sourceSize;
    if (!size.isValid()) {
        pixels.error = u"Missing/unknown cover dimensions"_s;
        return { };
    }
    if (job.generation != state->generation.load() || !job.wanted->load())
        return { };
    if (qMax(size.width(), size.height()) > job.resolution)
        reader.setScaledSize(size.scaled(job.resolution, job.resolution, Qt::KeepAspectRatio));
    auto source = reader.read();
    pixels.readerReturnedBytes = source.sizeInBytes();
    if (source.isNull())
        pixels.error = reader.errorString().isEmpty() ? u"Cover decoder returned no image"_s
                                                      : reader.errorString();
    return source;
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
    source = readSource(job, state, *pixels);
    if (job.generation != state->generation.load() || !job.wanted->load())
        return { };
    if (!source.isNull() && source.colorSpace().isValid())
        source.convertToColorSpace(QColorSpace::SRgb);
    if (!pixels->error.isEmpty())
        pixels->fallbackTitle = job.title;
    pixels->image = paint(source, job.resolution, job.title);
    pixels->image.setColorSpace(QColorSpace::SRgb);
    pixels->mipmaps = makeMipmaps(pixels->image);
    if (job.resolution > yalahoo::coverPixels::TileSide) {
        for (int y = 0; y < job.resolution; y += yalahoo::coverPixels::TileSide)
            for (int x = 0; x < job.resolution; x += yalahoo::coverPixels::TileSide) {
                CoverMipTile tile;
                tile.origin = { x, y };
                tile.image = pixels->image.copy(x, y, yalahoo::coverPixels::TileSide,
                                                yalahoo::coverPixels::TileSide);
                for (int level = 1; level <= pixels->mipmaps.size(); ++level) {
                    const auto &mip = pixels->mipmaps[level - 1];
                    if (level <= TileMipLevels)
                        tile.mipmaps.append(mip.copy(x >> level, y >> level,
                                                     yalahoo::coverPixels::TileSide >> level,
                                                     yalahoo::coverPixels::TileSide >> level));
                    else if (x == 0 && y == 0)
                        tile.mipmaps.append(mip);
                }
                pixels->tiles.append(std::move(tile));
            }
    }
    if (job.generation != state->generation.load() || !job.wanted->load())
        return { };
    return pixels;
}
// A completed decode may belong to a replaced request or a destroyed facade.
// Check identity and generation before caching or notifying the GUI consumer.
void CoverCache::complete(const Job &job, CoverSnapshot pixels)
{
    --d->m_active;
    d->m_workingSetBytes -= workingSetEstimateFor(job.resolution);
    if (d->m_running.value(job.key).wanted == job.wanted)
        d->m_running.remove(job.key);
    if (job.generation != generation() || !job.wanted->load() || !pixels) {
        ++d->m_stale;
    } else {
        const qint64 bytes = pixels->byteSize();
        const auto completedKey = pixels->error.isEmpty() && !job.path.isEmpty()
                ? key(job.identity, job.path, job.resolution, { })
                : job.key;
        if (const auto previous = d->m_cache.find(completedKey); previous != d->m_cache.end()) {
            d->m_bytes -= previous->pixels->byteSize();
            d->m_cache.erase(previous);
        }
        d->m_peakReaderBytes = qMax(d->m_peakReaderBytes, pixels->readerReturnedBytes);
        d->m_decoderScaledResults += pixels->decoderScaling && pixels->error.isEmpty();
        if (bytes <= cacheLimit) {
            while (d->m_bytes + bytes > cacheLimit && !d->m_cache.isEmpty()) {
                const auto oldest = std::min_element(d->m_cache.begin(), d->m_cache.end(),
                                                     [](const Entry &left, const Entry &right) {
                                                         return left.used < right.used;
                                                     });
                d->m_bytes -= oldest->pixels->byteSize();
                d->m_cache.erase(oldest);
            }
            d->m_cache.insert(completedKey, { pixels, ++d->m_clock });
            d->m_bytes += bytes;
        }
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
             { u"suspended"_s, d->m_suspended },
             { u"pending"_s, d->m_pending.size() },
             { u"active"_s, d->m_active },
             { u"peakPending"_s, d->m_peakPending },
             { u"peakActive"_s, d->m_peakActive },
             { u"peakInFlightWorkingSetEstimateBytes"_s, d->m_peakWorkingSetBytes },
             { u"imageAllocationLimitBytes"_s,
               QImageReader::allocationLimit() > 0
                       ? QJsonValue(qint64(QImageReader::allocationLimit()) * BytesPerMiB)
                       : QJsonValue(QJsonValue::Null) },
             { u"peakReaderReturnedBytesPerJob"_s, d->m_peakReaderBytes },
             { u"decoderScaledResults"_s, d->m_decoderScaledResults },
             { u"completedLruBytes"_s, d->m_bytes },
             { u"inFlightWorkingSetEstimateBytes"_s, d->m_workingSetBytes },
             { u"decodeStarts"_s, qint64(d->m_decodes) },
             { u"rejected"_s, qint64(d->m_rejected) },
             { u"stale"_s, qint64(d->m_stale) } };
}
bool CoverCache::hasPendingCapacity() const
{
    return d->m_pending.size() < pendingLimit;
}
}
