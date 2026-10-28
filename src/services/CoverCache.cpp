#include "CoverCache.h"
#include <utility>
#include "ColorTransfer.h"
#include "Units.h"
#include <atomic>
#include <QCoreApplication>
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
#include <new>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <list>

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
QImage allocateImage(QSize size, QImage::Format format, const CoverCache::ImageAllocator &allocator)
{
    auto image = allocator ? allocator(size, format) : QImage(size, format);
    if (image.isNull() || image.size() != size || image.format() != format)
        throw std::bad_alloc();
    return image;
}
QImage requireImage(QImage image)
{
    if (image.isNull())
        throw std::bad_alloc();
    return image;
}
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
// Halving an extent visits at most three source rows, plus floating endpoint slack.
constexpr auto MaximumReductionRows = 4;

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
template <class ReadRow, class ReadPixel>
void reduceMip(QSize currentSize, QImage &next, QImage &encoded, const ReadRow &readRow,
               const ReadPixel &read)
{
    const auto size = next.size();
    const double sx = double(currentSize.width()) / size.width();
    const double sy = double(currentSize.height()) / size.height();
    const int columns = currentSize.width() > 1 ? 2 : 1;
    const int rows = currentSize.height() > 1 ? 2 : 1;
    const bool integralFootprint = currentSize.width() == size.width() * columns
            && currentSize.height() == size.height() * rows;
    for (int y = 0; y < size.height(); ++y) {
        auto *output = reinterpret_cast<float *>(next.scanLine(y));
        auto *bytes = encoded.scanLine(y);
        const double y0 = y * sy, y1 = std::min(double(currentSize.height()), (y + 1) * sy);
        const int endY = int(std::ceil(y1));
        const int firstY = int(y0);
        std::array<decltype(readRow(0)), MaximumReductionRows> sourceRows;
        for (int iy = firstY; iy < endY; ++iy)
            sourceRows[iy - firstY] = readRow(iy);
        for (int x = 0; x < size.width(); ++x) {
            std::array<double, 4> sum{ };
            const double x0 = x * sx, x1 = std::min(double(currentSize.width()), (x + 1) * sx);
            const int endX = int(std::ceil(x1));
            if (integralFootprint) {
                // Power-of-two cover layers have unit source weights. Preserve the same
                // double accumulation order while avoiding per-sample overlap arithmetic.
                for (int iy = 0; iy < rows; ++iy) {
                    for (int ix = 0; ix < columns; ++ix) {
                        const auto sample = read(sourceRows[iy], x * columns + ix);
                        for (int c = 0; c < 4; ++c)
                            sum[c] += sample[c];
                    }
                }
            } else {
                for (int iy = int(y0); iy < endY; ++iy) {
                    const double weightY = std::min(y1, iy + 1.0) - std::max(y0, double(iy));
                    for (int ix = int(x0); ix < endX; ++ix) {
                        const double weight =
                                (std::min(x1, ix + 1.0) - std::max(x0, double(ix))) * weightY;
                        const auto sample = read(sourceRows[iy - firstY], ix);
                        for (int c = 0; c < 4; ++c)
                            sum[c] += sample[c] * weight;
                    }
                }
            }
            for (int c = 0; c < 4; ++c)
                output[4 * x + c] = float(sum[c] / (sx * sy));
            const double alpha = output[4 * x + 3];
            for (int c = 0; c < 3; ++c)
                bytes[4 * x + c] = uchar(std::clamp(
                        qRound(alpha > 0 ? colorTransfer::encode(output[4 * x + c] / alpha) * alpha
                                               * yalahoo::units::MaximumChannelByte
                                         : 0),
                        0, yalahoo::units::MaximumChannelByte));
            bytes[4 * x + 3] = uchar(std::clamp(qRound(alpha * yalahoo::units::MaximumChannelByte),
                                                0, yalahoo::units::MaximumChannelByte));
        }
    }
}

}
qint64 CoverPixels::byteSize() const
{
    qint64 bytes = 0;
    QSet<qint64> seen;
    const auto count = [&](const QImage &pixels) {
        if (pixels.isNull())
            return;
        const auto previous = seen.size();
        seen.insert(pixels.cacheKey());
        if (seen.size() != previous)
            bytes += pixels.sizeInBytes();
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

struct CoverCache::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    explicit Private(CoverCache *owner) : q(owner)
    {
    }
    CoverCache *const q;

    struct Job final
    {
        QString key, identity, path, title;
        int resolution = yalahoo::coverPixels::TileSide;
        quint64 generation = 0;
        std::shared_ptr<std::atomic_bool> wanted;
    };
    struct Entry final
    {
        QString key;
        CoverSnapshot pixels;
        qint64 bytes = 0;
    };
    struct State final
    {
        Q_DISABLE_COPY_MOVE(State)
        State() = default;
        QMutex mutex;
        CoverCache *receiver = nullptr;
        ImageAllocator allocator;
        std::atomic<quint64> generation{ 1 };
    };
    static void decodeAndDeliver(const Job &job, const std::shared_ptr<State> &state);
    static QImage readSource(const Job &job, const std::shared_ptr<State> &state,
                             CoverPixels &pixels);
    static CoverSnapshot decode(const Job &job, const std::shared_ptr<State> &state);
    static QString key(const QString &identity, const QString &path, int resolution,
                       const QString &title);
    CoverSnapshot lookupCached(const QString &cacheKey, bool requireArtwork = false);
    void pump();
    void complete(const Job &job, const CoverSnapshot &pixels);
    std::shared_ptr<State> m_state;
    QTimer m_retry;
    QList<Job> m_pending;
    QSet<QString> m_pendingKeys;
    QHash<QString, Job> m_running;
    // Hash iterators may move on rehash; list iterators survive admission and LRU splicing.
    std::list<Entry> m_lru;
    QHash<QString, std::list<Entry>::iterator> m_cache;
    qint64 m_bytes = 0, m_workingSetBytes = 0, m_peakWorkingSetBytes = 0;
    bool m_suspended = false;
    int m_active = 0, m_peakActive = 0, m_peakPending = 0;
    quint64 m_decodes = 0, m_rejected = 0, m_stale = 0;
    qint64 m_peakReaderBytes = 0;
    int m_decoderScaledResults = 0;
};
CoverCache::CoverCache(QObject *parent, ImageAllocator allocator)
    : QObject(parent), d(std::make_unique<Private>(this))
{
    d->m_state = std::make_shared<Private::State>();
    d->m_state->receiver = this;
    d->m_state->allocator = std::move(allocator);
    d->m_retry.setSingleShot(true);
    d->m_retry.setInterval(DecodeRetryMilliseconds);
    connect(&d->m_retry, &QTimer::timeout, this, [this] {
        d->pump();
    });
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
QString CoverCache::Private::key(const QString &identity, const QString &path, int resolution,
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
    if (!path.isEmpty() && !title.isEmpty()) {
        if (auto artwork = d->lookupCached(d->key(identity, path, resolution, { }), true))
            return artwork;
    }
    return d->lookupCached(d->key(identity, path, resolution, title));
}
CoverSnapshot CoverCache::Private::lookupCached(const QString &cacheKey, bool requireArtwork)
{
    const auto it = m_cache.constFind(cacheKey);
    if (it == m_cache.cend())
        return { };
    const auto entry = *it;
    if (requireArtwork && !entry->pixels->error.isEmpty())
        return { };
    m_lru.splice(m_lru.begin(), m_lru, entry);
    return entry->pixels;
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
    if (!path.isEmpty() && !title.isEmpty()) {
        if (d->lookupCached(d->key(identity, path, resolution, { }), true))
            return true;
    }
    const auto cacheKey = d->key(identity, path, resolution, title);
    if (d->lookupCached(cacheKey))
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
    d->pump();
    return true;
}
void CoverCache::reset()
{
    ++d->m_state->generation;
    d->m_pending.clear();
    d->m_pendingKeys.clear();
    d->m_cache.clear();
    d->m_lru.clear();
    d->m_bytes = 0;
    // Running jobs retain their reservations until their completion is consumed.
    d->m_retry.stop();
}
void CoverCache::cancel(const QString &identity)
{
    d->m_pending.removeIf([this, &identity](const Private::Job &job) {
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
    for (auto it = d->m_lru.begin(); it != d->m_lru.end();) {
        if (it->pixels->identity == identity) {
            d->m_bytes -= it->bytes;
            d->m_cache.remove(it->key);
            it = d->m_lru.erase(it);
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
        d->pump();
}
void CoverCache::Private::decodeAndDeliver(const Private::Job &job,
                                           const std::shared_ptr<Private::State> &state)
{
    const auto pixels = decode(job, state);
    const QMutexLocker lock(&state->mutex);
    if (state->receiver) {
        auto *receiver = state->receiver;
        QMetaObject::invokeMethod(
                receiver,
                [receiver, job, pixels] {
                    receiver->d->complete(job, pixels);
                },
                Qt::QueuedConnection);
    }
}
void CoverCache::Private::pump()
{
    if (m_suspended)
        return;
    while (m_active < concurrency && !m_pending.isEmpty()) {
        const auto job = m_pending.first();
        const auto state = m_state;
        const bool started = decodePool().tryStart([job, state] {
            decodeAndDeliver(job, state);
        });
        if (!started) {
            m_retry.start();
            return;
        }
        m_pending.removeFirst();
        m_pendingKeys.remove(job.key);
        m_running.insert(job.key, job);
        ++m_active;
        m_workingSetBytes += CoverCache::workingSetEstimateFor(job.resolution);
        m_peakWorkingSetBytes = std::max(m_peakWorkingSetBytes, m_workingSetBytes);
        ++m_decodes;
        m_peakActive = qMax(m_peakActive, m_active);
    }
}
qint64 CoverCache::workingSetEstimateFor(int resolution)
{
    // Requested paint/mip working storage only. Source decoder and color-conversion
    // allocations are independent and are not a process-memory reservation.
    return qint64(resolution) * resolution * WorkingBytesPerOutputPixel;
}
QImage CoverCache::paint(const QImage &source, int resolution, const QString &title,
                         const ImageAllocator &allocator)
{
    if (resolution <= 0 || (resolution & (resolution - 1)) != 0)
        throw std::invalid_argument("Invalid cover paint resolution");
    auto image = allocateImage({ resolution, resolution }, QImage::Format_RGBA8888_Premultiplied,
                               allocator);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    if (!painter.isActive())
        throw std::bad_alloc();
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
QList<QImage> CoverCache::makeMipmaps(const QImage &image, const ImageAllocator &allocator)
{
    // Decode sRGB before area reduction; alpha remains premultiplied in linear light.
    // Independent chains never incorporate pixels belonging to another atlas slot.
    if (image.isNull())
        return { };
    const auto source = requireImage(image.convertToFormat(QImage::Format_RGBA8888));
    static const auto decoded = [] {
        std::array<float, ByteChannelValues> values;
        for (int i = 0; i < ByteChannelValues; ++i)
            values[i] = float(colorTransfer::decode(i / ByteChannelScale));
        return values;
    }();
    QImage linear;
    QList<QImage> levels;
    levels.reserve(std::bit_width(unsigned(qMax(source.width(), source.height()))));
    QSize currentSize = source.size();
    while (currentSize.width() > 1 || currentSize.height() > 1) {
        const QSize size(qMax(1, currentSize.width() / 2), qMax(1, currentSize.height() / 2));
        auto next = allocateImage(size, QImage::Format_RGBA32FPx4_Premultiplied, allocator);
        auto encoded = allocateImage(size, QImage::Format_RGBA8888_Premultiplied, allocator);
        encoded.setColorSpace(QColorSpace::SRgb);
        if (linear.isNull()) {
            // Fuse the full-resolution decode with the first reduction. Preserve the
            // original float premultiplication before double accumulation; no full-size
            // 16-byte/pixel linear intermediate needs to be stored or read back.
            const auto readRow = [&source](int y) {
                return source.constScanLine(y);
            };
            reduceMip(currentSize, next, encoded, readRow, [](const uchar *row, int x) {
                const auto *input = row + 4 * x;
                const float alpha = input[3] / FloatByteChannelScale;
                return std::array<float, 4>{ decoded[input[0]] * alpha, decoded[input[1]] * alpha,
                                             decoded[input[2]] * alpha, alpha };
            });
        } else {
            const auto readRow = [&linear](int y) {
                return reinterpret_cast<const float *>(linear.constScanLine(y));
            };
            reduceMip(currentSize, next, encoded, readRow, [](const float *row, int x) {
                const auto *input = row + 4 * x;
                return std::array<float, 4>{ input[0], input[1], input[2], input[3] };
            });
        }
        levels.append(std::move(encoded));
        currentSize = size;
        linear = std::move(next);
    }
    return levels;
}
// Decode into worker-owned pixels, then publish an immutable snapshot.
// Cancellation/generation checks protect the facade; decoded byte limits protect allocation.
QImage CoverCache::Private::readSource(const Private::Job &job,
                                       const std::shared_ptr<Private::State> &state,
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
CoverSnapshot CoverCache::Private::decode(const Private::Job &job,
                                          const std::shared_ptr<Private::State> &state)
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
    try {
        if (!source.isNull() && source.colorSpace().isValid())
            source = requireImage(source.convertedToColorSpace(QColorSpace::SRgb));
        if (!pixels->error.isEmpty())
            pixels->fallbackTitle = job.title;
        pixels->image = CoverCache::paint(source, job.resolution, job.title, state->allocator);
        pixels->image.setColorSpace(QColorSpace::SRgb);
        pixels->mipmaps = CoverCache::makeMipmaps(pixels->image, state->allocator);
        if (job.resolution > yalahoo::coverPixels::TileSide) {
            const qsizetype tileSide = job.resolution / yalahoo::coverPixels::TileSide;
            pixels->tiles.reserve(tileSide * tileSide);
            for (int y = 0; y < job.resolution; y += yalahoo::coverPixels::TileSide)
                for (int x = 0; x < job.resolution; x += yalahoo::coverPixels::TileSide) {
                    CoverMipTile tile;
                    tile.origin = { x, y };
                    tile.image = requireImage(pixels->image.copy(
                            x, y, yalahoo::coverPixels::TileSide, yalahoo::coverPixels::TileSide));
                    tile.mipmaps.reserve(x == 0 && y == 0 ? pixels->mipmaps.size()
                                                          : std::min(qsizetype(TileMipLevels),
                                                                     pixels->mipmaps.size()));
                    for (int level = 1; level <= pixels->mipmaps.size(); ++level) {
                        const auto &mip = pixels->mipmaps[level - 1];
                        if (level <= TileMipLevels)
                            tile.mipmaps.append(requireImage(mip.copy(
                                    x >> level, y >> level, yalahoo::coverPixels::TileSide >> level,
                                    yalahoo::coverPixels::TileSide >> level)));
                        else if (x == 0 && y == 0)
                            tile.mipmaps.append(mip);
                    }
                    pixels->tiles.append(std::move(tile));
                }
        }
    } catch (const std::bad_alloc &) {
        pixels->error = u"Cover image allocation failed"_s;
        pixels->fallbackTitle = job.title;
        pixels->image = { };
        pixels->mipmaps.clear();
        pixels->tiles.clear();
    }
    if (job.generation != state->generation.load() || !job.wanted->load())
        return { };
    return pixels;
}
// A completed decode may belong to a replaced request or a destroyed facade.
// Check identity and generation before caching or notifying the GUI consumer.
void CoverCache::Private::complete(const Private::Job &job, const CoverSnapshot &pixels)
{
    --m_active;
    m_workingSetBytes -= CoverCache::workingSetEstimateFor(job.resolution);
    if (const auto running = m_running.find(job.key);
        running != m_running.end() && running->wanted == job.wanted)
        m_running.erase(running);
    if (job.generation != q->generation() || !job.wanted->load() || !pixels) {
        ++m_stale;
    } else {
        const qint64 bytes = pixels->byteSize();
        const auto completedKey = pixels->error.isEmpty() && !job.path.isEmpty()
                ? key(job.identity, job.path, job.resolution, { })
                : job.key;
        if (const auto previous = m_cache.find(completedKey); previous != m_cache.end()) {
            m_bytes -= (*previous)->bytes;
            m_lru.erase(*previous);
            m_cache.erase(previous);
        }
        m_peakReaderBytes = qMax(m_peakReaderBytes, pixels->readerReturnedBytes);
        m_decoderScaledResults += pixels->decoderScaling && pixels->error.isEmpty();
        if (bytes <= cacheLimit) {
            while (m_bytes + bytes > cacheLimit && !m_cache.isEmpty()) {
                const auto &oldest = m_lru.back();
                m_bytes -= oldest.bytes;
                m_cache.remove(oldest.key);
                m_lru.pop_back();
            }
            m_lru.push_front({ completedKey, pixels, bytes });
            m_cache.insert(completedKey, m_lru.begin());
            m_bytes += bytes;
        }
        const QPointer<CoverCache> guard(q);
        Q_EMIT q->ready(pixels);
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
