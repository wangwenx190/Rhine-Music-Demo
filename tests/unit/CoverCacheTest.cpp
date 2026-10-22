// Exercises bounded decode queues, stale generations, color-correct mipmaps and cache ownership.
// Explicit fixture literals are independent inputs or expected results; timing bounds have named
// constants.

#include "SceneAdapter.h"
#include "MotionController.h"
#include "SelectionController.h"
#include <limits>
#include <stdexcept>
#include "CoverCache.h"
#include "AtlasAllocator.h"
#include "SceneItem.h"
#include <QQuickWindow>
#include <QFile>
#include <QColorSpace>
#include <QImageReader>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QtTest>

namespace {
// Bounds for asynchronous completion and event delivery; fixture values remain independent.
constexpr int AsyncDeadlineMilliseconds = 10000;
constexpr int WorkerDrainDeadlineMilliseconds = 30000;
}

using namespace Qt::StringLiterals;
using namespace yalahoo;
using namespace yalahoo::rhi;

class HiddenSceneItem final : public SceneItem
{
    Q_DISABLE_COPY_MOVE(HiddenSceneItem)
public:
    HiddenSceneItem() = default;
    using SceneItem::SceneItem;
    void publishNow()
    {
        updatePolish();
    }
};

class CoverCacheTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(CoverCacheTest)
public:
    CoverCacheTest() = default;
    ~CoverCacheTest() override
    {
        CoverCache::finishDecoders();
    }

private:
    static CoverSnapshot pixels(QString id, int resolution = 256, quint64 generation = 1)
    {
        auto p = std::make_shared<CoverPixels>();
        p->identity = std::move(id);
        p->generation = generation;
        p->resolution = resolution;
        p->image = QImage(resolution, resolution, QImage::Format_RGBA8888_Premultiplied);
        p->image.fill(Qt::red);
        return p;
    }
private Q_SLOTS:
    void failedDetailedUpgradeReusesPublishedDescriptors()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath(u"cover.png"_s);
        QImage image(32, 32, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(path));
        LibrarySnapshot library;
        Album album;
        album.id = u"one"_s;
        album.title = u"One"_s;
        album.coverUrl = QUrl::fromLocalFile(path);
        library.albums.append(album);
        MotionController motion;
        SelectionController selection(&motion);
        selection.setColumns({ { u"column"_s, u"Column"_s, { album.id } } },
                             std::make_shared<const LibrarySnapshot>(library));
        SceneAdapter adapter;
        adapter.setSelection(&selection);
        QQuickWindow window;
        HiddenSceneItem item(window.contentItem());
        item.setSize({ 320, 240 });
        item.setAdapter(&adapter);
        item.publishNow();
        QTRY_VERIFY_WITH_TIMEOUT(
                (item.publishNow(), item.coverStatistics()[u"selectedDetailed"_s].toBool()),
                AsyncDeadlineMilliseconds);
        const auto initial = item.coverStatistics();
        const auto originalResolution =
                initial[u"owners"_s].toArray().first().toObject()[u"resolution"_s].toInt();
        QVERIFY(QFile::remove(path));
        // The already-decoded artwork remains usable when the backing file disappears.
        item.setSize({ 1920, 1440 });
        item.publishNow();
        QTRY_VERIFY_WITH_TIMEOUT(
                (item.publishNow(),
                 item.coverStatistics()[u"cache"_s].toObject()[u"decodeStarts"_s].toInteger()
                                 > initial[u"cache"_s].toObject()[u"decodeStarts"_s].toInteger()
                         && item.coverStatistics()[u"cache"_s].toObject()[u"active"_s].toInt() == 0
                         && item.coverStatistics()[u"cache"_s].toObject()[u"pending"_s].toInt()
                                 == 0),
                AsyncDeadlineMilliseconds);
        item.publishNow();
        QVERIFY(!item.coverStatistics()[u"selectedDetailed"_s].toBool());
        QCOMPARE(item.coverStatistics()[u"owners"_s]
                         .toArray()
                         .first()
                         .toObject()[u"resolution"_s]
                         .toInt(),
                 originalResolution);
        const auto previous = item.input();
        selection.setLift(.41);
        item.publishNow();
        QVERIFY(item.input().entities.last() != previous.entities.last());
        QVERIFY(item.input().coverTiles.isSharedWith(previous.coverTiles));
        QVERIFY(item.input().coverLeases.isSharedWith(previous.coverLeases));
        QVERIFY(item.input().atlasUploads.isSharedWith(previous.atlasUploads));
        QVERIFY(!window.isVisible());
    }
    void readOnlySubmissionPreservesSharedDescriptors()
    {
        RhiItem item;
        Input input;
        input.atlasGeneration = 1;
        input.atlasSize = { 512, 256 };
        input.entities.resize(2);
        input.entityIds = { 1, 2 };
        input.coverTiles = { QRect(0, 0, 256, 256), QRect(256, 0, 256, 256) };
        input.fallbackCoverTiles = input.coverTiles;
        input.coverLeases = { 1, 2 };
        input.fallbackCoverLeases = input.coverLeases;
        input.shells = true;
        item.submit(input, { 16, 16 });
        QVERIFY(item.input().coverTiles.isSharedWith(input.coverTiles));
        QVERIFY(item.input().fallbackCoverTiles.isSharedWith(input.fallbackCoverTiles));
        QVERIFY(item.input().entityIds.isSharedWith(input.entityIds));
    }
    void shellIdentityValidationFollowsAcceptedInputs()
    {
        RhiItem item;
        Input input;
        input.cover = QImage(1, 1, QImage::Format_RGBA8888_Premultiplied);
        input.cover.fill(Qt::white);
        input.entities.resize(2);
        input.entityIds = { 1, 2 };
        input.shells = true;
        item.submit(input, { 16, 16 });
        input.entities[0].translate(1, 0, 0);
        item.submit(input, { 16, 16 });
        const auto accepted = item.submission();
        input.entityIds = { 1, 1 };
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, item.submit(input, { 16, 16 }));
        QCOMPARE(item.submission(), accepted);
        // Non-shell and empty-entity submissions do not prove uniqueness for a later shell frame.
        input.shells = false;
        item.submit(input, { 16, 16 });
        input.shells = true;
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, item.submit(input, { 16, 16 }));
        input.entities.clear();
        item.submit(input, { 16, 16 });
        input.entities.resize(2);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, item.submit(input, { 16, 16 }));
        input.entityIds = { 2, 1 };
        item.submit(input, { 16, 16 });
    }
    void mipmapsPreserveLinearLightAndAlpha()
    {
        QImage checker(2, 2, QImage::Format_RGBA8888_Premultiplied);
        checker.fill(Qt::black);
        checker.setPixelColor(0, 0, Qt::white);
        checker.setPixelColor(1, 1, Qt::white);
        auto levels = CoverCache::makeMipmaps(checker);
        QCOMPARE(levels.size(), 1);
        QCOMPARE(levels.first().size(), QSize(1, 1));
        const auto average = levels.first().pixelColor(0, 0);
        QVERIFY(std::abs(average.red() - 188) <= 1);
        QCOMPARE(average.alpha(), 255);
        checker.fill(Qt::transparent);
        checker.setPixelColor(0, 0, Qt::white);
        levels = CoverCache::makeMipmaps(checker);
        const auto edge = levels.first().pixelColor(0, 0);
        QVERIFY(edge.red() >= 253);
        QVERIFY(std::abs(edge.alpha() - 64) <= 1);
        // Single-axis chains still integrate every source texel; odd widths use area weights.
        for (const auto size : { QSize(4, 1), QSize(1, 4) }) {
            QImage strip(size, QImage::Format_RGBA8888_Premultiplied);
            strip.fill(Qt::black);
            strip.setPixelColor(0, 0, Qt::white);
            strip.setPixelColor(size.width() - 1, size.height() - 1, Qt::white);
            const auto chain = CoverCache::makeMipmaps(strip);
            QCOMPARE(chain.size(), 2);
            QVERIFY(std::abs(chain.last().pixelColor(0, 0).red() - 188) <= 1);
        }
        QImage odd(3, 1, QImage::Format_RGBA8888_Premultiplied);
        odd.fill(Qt::black);
        odd.setPixelColor(2, 0, Qt::white);
        levels = CoverCache::makeMipmaps(odd);
        QCOMPARE(levels.size(), 1);
        QVERIFY(std::abs(levels.first().pixelColor(0, 0).red() - 156) <= 1);
        QImage oddAlpha(3, 3, QImage::Format_RGBA8888_Premultiplied);
        for (int y = 0; y < 3; ++y) {
            oddAlpha.setPixelColor(0, y, Qt::white);
            oddAlpha.setPixelColor(1, y, Qt::black);
            oddAlpha.setPixelColor(2, y, QColor(255, 0, 0, 128));
        }
        const auto mixed = CoverCache::makeMipmaps(oddAlpha);
        QCOMPARE(mixed.size(), 1);
        // Linear premultiplied mean: alpha=(1+1+128/255)/3,
        // red=(1+128/255)/3, green=blue=1/3, then encode and premultiply.
        const auto *rgba = mixed.first().constBits();
        const std::array<int, 4> expected{ 170, 142, 142, 213 };
        for (int c = 0; c < 4; ++c)
            QVERIFY(std::abs(int(rgba[c]) - expected[c]) <= 1);
        QVERIFY(CoverCache::makeMipmaps(QImage()).isEmpty());
        QVERIFY(CoverCache::makeMipmaps(QImage(1, 1, QImage::Format_RGBA8888)).isEmpty());
        // The final 27 -> 13 footprint can round just beyond its source extent.
        for (const auto size : { QSize(27, 1), QSize(1, 27), QSize(27, 27), QSize(55, 27) }) {
            QImage uniform(size, QImage::Format_RGBA8888);
            uniform.fill(QColor(127, 63, 31, 128));
            for (const auto &mip : CoverCache::makeMipmaps(uniform)) {
                const auto edge = mip.pixelColor(mip.width() - 1, mip.height() - 1);
                QVERIFY(std::abs(edge.red() - 127) <= 1);
                QVERIFY(std::abs(edge.green() - 63) <= 1);
                QVERIFY(std::abs(edge.blue() - 31) <= 1);
                QCOMPARE(edge.alpha(), 128);
            }
        }
    }
    void atlasExpansionRetainsPixelsAndLeaseIdentity()
    {
        AtlasAllocator atlas(8192, 436, 24);
        auto lease = atlas.acquire(u"one"_s);
        const auto cover = pixels(u"one"_s);
        QVERIFY(atlas.publish(lease, cover));
        const auto before = atlas.replay(0);
        const auto generation = atlas.generation();
        atlas.advanceGeneration();
        QVERIFY(atlas.generation() != generation);
        lease.generation = atlas.generation();
        const auto after = atlas.replay(0);
        QCOMPARE(after.size(), before.size());
        QCOMPARE(after.first().lease, before.first().lease);
        QCOMPARE(after.first().pixels.cacheKey(), before.first().pixels.cacheKey());
        QCOMPARE(after.first().generation, atlas.generation());
        QVERIFY(atlas.publish(lease, cover));
        atlas.release(lease);
        QCOMPARE(atlas.retainedBytes(), 0);
    }
    void atlasReplayTracksStrideReleaseAndReplacement()
    {
        AtlasAllocator atlas(512, 4, 2);
        const auto first = atlas.acquire(u"first"_s);
        const auto second = atlas.acquire(u"second"_s);
        const auto third = atlas.acquire(u"third"_s);
        QVERIFY(atlas.publish(third, pixels(u"third"_s)));
        const auto original = atlas.replay(0, 4);
        QCOMPARE(original.size(), 1);
        QCOMPARE(original.first().slot, 4);
        QCOMPARE(atlas.replay(0).first().slot, 2);
        QCOMPARE(atlas.replay(0, 4).first().slot, 4);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 atlas.replay(0, std::numeric_limits<int>::max()));
        atlas.release(third);
        QVERIFY(atlas.replay(0, 4).isEmpty());
        const auto replacement = atlas.acquire(u"replacement"_s);
        QVERIFY(replacement.lease != third.lease);
        QVERIFY(atlas.publish(replacement, pixels(u"replacement"_s)));
        const auto updated = atlas.replay(0, 4);
        QCOMPARE(updated.first().lease, replacement.lease);
        QCOMPARE(original.first().lease, third.lease);
        QVERIFY(updated.first().pixels.cacheKey() != original.first().pixels.cacheKey());
        atlas.reset();
        QVERIFY(atlas.replay(0, 4).isEmpty());
        QCOMPARE(original.first().pixels.pixelColor(0, 0), QColor(Qt::red));
        Q_UNUSED(first);
        Q_UNUSED(second);
    }
    void recycledSlotPreservesRelocatedIdentity()
    {
        AtlasAllocator atlas(512, 3, 2);
        const auto first = atlas.acquire(u"first"_s);
        const auto second = atlas.acquire(u"second"_s);
        atlas.release(first);
        atlas.release(second);
        const auto relocated = atlas.acquire(u"second"_s);
        QCOMPARE(relocated.slot, first.slot);
        const auto replacement = atlas.acquire(u"replacement"_s);
        QCOMPARE(replacement.slot, second.slot);
        const auto retained = atlas.acquire(u"second"_s);
        QCOMPARE(retained.slot, relocated.slot);
        QCOMPARE(retained.lease, relocated.lease);
        QVERIFY(atlas.publish(retained, pixels(u"second"_s)));
        atlas.release(relocated);
        QVERIFY(atlas.publish(retained, pixels(u"second"_s)));
        atlas.release(retained);
        QVERIFY(!atlas.publish(retained, pixels(u"second"_s)));
        const auto reused = atlas.acquire(u"third"_s);
        QCOMPARE(reused.slot, first.slot);
        atlas.reset();
        QCOMPARE(atlas.acquire(u"reset"_s).slot, 0);
    }
    void malformedAtlasLeaseCannotOverflowSlotIndex()
    {
        AtlasAllocator atlas(2048, 1);
        const auto valid = atlas.acquire(u"one"_s);
        auto invalid = valid;
        invalid.page = std::numeric_limits<int>::max();
        atlas.release(invalid);
        QVERIFY(!atlas.publish(invalid, { }));
        atlas.release(valid);
        QVERIFY(atlas.acquire(u"two"_s).page >= 0);
    }
    void closingCacheBeforeFinalDecoderJoin()
    {
        auto cache = std::make_unique<CoverCache>();
        QSignalSpy ready(cache.get(), &CoverCache::ready);
        for (int i = 0; i < CoverCache::concurrency; ++i)
            QVERIFY(cache->request(QString::number(i), { }, 1024, u"anonymous"_s));
        cache.reset();
        CoverCache::finishDecoders();
        QCoreApplication::sendPostedEvents();
        QCOMPARE(ready.size(), 0);
    }
    void coldHiddenAndOpaqueRootDoesNotDecode()
    {
        MotionController motion;
        SelectionController selection(&motion);
        selection.setColumns({ { u"column"_s, u"column"_s, { u"a"_s, u"b"_s } } });
        SceneAdapter adapter;
        adapter.setSelection(&selection);
        QQuickWindow window;
        HiddenSceneItem item(window.contentItem());
        item.setSize({ 640, 480 });
        item.setAdapter(&adapter);
        QCOMPARE(item.coverStatistics()[u"cache"_s].toObject()[u"decodeStarts"_s].toInt(), 0);
        item.setRenderingActive(false);
        item.setSceneOccluded(true);
        item.publishNow();
        QCOMPARE(item.submission(), 0);
        item.setRenderingActive(true);
        item.publishNow();
        QVERIFY(item.input().sceneOccluded);
        QCOMPARE(item.coverStatistics()[u"cache"_s].toObject()[u"decodeStarts"_s].toInt(), 0);
        item.setSceneOccluded(false);
        item.publishNow();
        QVERIFY(item.input().poolFrame);
        QVERIFY(!item.input().sceneOccluded);
        QVERIFY(item.coverStatistics()[u"cache"_s].toObject()[u"decodeStarts"_s].toInt() > 0);
    }
    void telemetryFromPreviousVisibilityIsRejected()
    {
        SceneItem item;
        item.setTelemetryEnabled(true);
        const auto previous = item.profiled;
        QVERIFY(bool(previous));
        QSignalSpy submitted(&item, &SceneItem::frameSubmitted);
        previous({ { u"submission"_s, 1 } });
        item.setRenderingActive(false);
        QVERIFY(!item.profiled);
        item.setRenderingActive(true);
        QCoreApplication::sendPostedEvents();
        QCOMPARE(submitted.size(), 0);
        item.profiled({ { u"submission"_s, 2 } });
        QTRY_COMPARE(submitted.size(), 1);
        QCOMPARE(submitted[0][0].toJsonObject()[u"submission"_s].toInt(), 2);
    }
    void telemetryStopsBatchWhenReceiverDisablesIt()
    {
        SceneItem item;
        item.setTelemetryEnabled(true);
        QSignalSpy submitted(&item, &SceneItem::frameSubmitted);
        connect(&item, &SceneItem::frameSubmitted, &item, [&] {
            item.setTelemetryEnabled(false);
        });
        item.profiled({ { u"submission"_s, 1 } });
        item.profiled({ { u"submission"_s, 2 } });
        QCoreApplication::sendPostedEvents();
        QCOMPARE(submitted.size(), 1);
        QVERIFY(!item.telemetryEnabled());
    }
    void suspendedQueueDoesNotStartWorkers()
    {
        CoverCache cache;
        cache.setSuspended(true);
        QSignalSpy ready(&cache, &CoverCache::ready);
        for (int i = 0; i < 20; ++i)
            QVERIFY(cache.request(QString::number(i), { }, 256, u"anonymous"_s));
        QCoreApplication::sendPostedEvents();
        QCOMPARE(cache.statistics()[u"decodeStarts"_s].toInt(), 0);
        QCOMPARE(cache.statistics()[u"pending"_s].toInt(), 20);
        cache.setSuspended(false);
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 20, AsyncDeadlineMilliseconds);
        QCOMPARE(cache.statistics()[u"pending"_s].toInt(), 0);
        QCOMPARE(cache.statistics()[u"active"_s].toInt(), 0);
        QVERIFY(cache.statistics()[u"peakActive"_s].toInt() <= CoverCache::concurrency);
    }

    void adapterDestroyedDuringDecode()
    {
        MotionController motion;
        SelectionController selection(&motion);
        LibraryColumn column;
        column.albumIds = { u"a"_s, u"b"_s, u"c"_s };
        selection.setColumns({ column });
        auto adapter = std::make_unique<SceneAdapter>();
        adapter->setSelection(&selection);
        QQuickWindow window;
        HiddenSceneItem item(window.contentItem());
        item.setSize({ 960, 640 });
        item.setAdapter(adapter.get());
        item.publishNow();
        QVERIFY(item.input().poolFrame);
        QVERIFY(item.coverStatistics()[u"cache"_s].toObject()[u"peakActive"_s].toInt() > 0);
        adapter.reset();
        QVERIFY(!item.adapter());
        QTRY_COMPARE_WITH_TIMEOUT(
                item.coverStatistics()[u"cache"_s].toObject()[u"active"_s].toInt(), 0,
                AsyncDeadlineMilliseconds);
        QVERIFY(!window.isVisible());
    }
    void containAndTransparency()
    {
        for (const auto size : { QSize(100, 100), QSize(100, 200), QSize(200, 100) }) {
            QImage source(size, QImage::Format_RGBA8888);
            source.fill(QColor(200, 40, 80, 128));
            for (int resolution : { 256, 1024 }) {
                const auto p = CoverCache::paint(source, resolution, { });
                QCOMPARE(p.size(), QSize(resolution, resolution));
                QCOMPARE(p.pixelColor(0, 0).alpha(), 0);
                QCOMPARE(p.pixelColor(resolution / 2, resolution / 2).alpha(), 128);
                const int margin = resolution / 64;
                if (size.width() == size.height()) {
                    QCOMPARE(p.pixelColor(margin - 1, resolution / 2).alpha(), 0);
                    QCOMPARE(p.pixelColor(margin, resolution / 2).alpha(), 128);
                } else if (size.width() < size.height()) {
                    QCOMPARE(p.pixelColor(resolution / 4 - 1, resolution / 2).alpha(), 0);
                } else {
                    QCOMPARE(p.pixelColor(resolution / 2, resolution / 4 - 1).alpha(), 0);
                }
            }
        }
    }
    void boundedAsyncAndLru()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QImage large(2048, 2048, QImage::Format_RGB32);
        for (int i = 0; i < 96; ++i) {
            large.fill(QColor::fromHsv(i * 3, 180, 210));
            QVERIFY(large.save(directory.filePath(QString::number(i) + u".jpg"_s)));
        }
        CoverCache cache;
        QSet<QString> ready;
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        bool valid = true, scaling = true;
        connect(&cache, &CoverCache::ready, this, [&](CoverSnapshot p) {
            ready.insert(p->identity);
            valid = valid && p->error.isEmpty() && p->image.size() == QSize(256, 256)
                    && p->image.pixelColor(0, 0).alpha() == 0;
            scaling = scaling && p->decoderScaling;
            if (ready.size() == 96)
                loop.quit();
        });
        for (int i = 0; i < 96; ++i) {
            const auto id = QString::number(i);
            QVERIFY(cache.request(id, directory.filePath(id + u".jpg"_s), 256));
            QVERIFY(cache.request(id, directory.filePath(id + u".jpg"_s), 256));
        }
        timeout.start(WorkerDrainDeadlineMilliseconds);
        loop.exec();
        QCOMPARE(ready.size(), 96);
        const auto stats = cache.statistics();
        QVERIFY(valid);
        QVERIFY(scaling);
        QCOMPARE(stats[u"decodeStarts"_s].toInteger(), 96);
        QVERIFY(stats[u"peakActive"_s].toInt() <= 3);
        QVERIFY(stats[u"peakPending"_s].toInt() <= 96);
        QVERIFY(stats[u"completedLruBytes"_s].toInteger() <= CoverCache::cacheLimit);
        QCOMPARE(stats[u"inFlightWorkingSetEstimateBytes"_s].toInteger(), 0);
        const auto cached = cache.lookup(u"95"_s, directory.filePath(u"95.jpg"_s), 256);
        QVERIFY(cached);
        QVERIFY(cache.request(u"95"_s, directory.filePath(u"95.jpg"_s), 256));
        QCOMPARE(cache.statistics()[u"decodeStarts"_s], stats[u"decodeStarts"_s]);
    }
    void lruHitsAndTargetedInvalidationPreserveOrder()
    {
        QTemporaryDir directory;
        QImage image(8, 8, QImage::Format_RGB32);
        image.fill(Qt::blue);
        const auto path = directory.filePath(u"cover.png"_s);
        QVERIFY(image.save(path));
        CoverCache cache;
        QSignalSpy ready(&cache, &CoverCache::ready);
        QVERIFY(cache.request(u"0"_s, path, 512));
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, AsyncDeadlineMilliseconds);
        const auto retained = qvariant_cast<CoverSnapshot>(ready.first().first());
        const auto bytes = retained->byteSize();
        const int capacity = int(CoverCache::cacheLimit / bytes);
        QVERIFY(capacity >= 3 && capacity <= 12);
        for (int i = 1; i < capacity; ++i) {
            QVERIFY(cache.request(QString::number(i), path, 512));
            QTRY_COMPARE_WITH_TIMEOUT(ready.size(), i + 1, AsyncDeadlineMilliseconds);
        }
        QCOMPARE(cache.lookup(u"0"_s, path, 512), retained);
        cache.invalidate(u"2"_s);
        QVERIFY(!cache.lookup(u"2"_s, path, 512));
        QCOMPARE(cache.statistics()[u"completedLruBytes"_s].toInteger(), (capacity - 1) * bytes);
        // Fill the freed entry and overflow once: the untouched oldest entry is 1, not 0.
        for (int i = capacity; i <= capacity + 1; ++i) {
            QVERIFY(cache.request(QString::number(i), path, 512));
            QTRY_COMPARE_WITH_TIMEOUT(ready.size(), i + 1, AsyncDeadlineMilliseconds);
        }
        QVERIFY(!cache.lookup(u"1"_s, path, 512));
        QCOMPARE(cache.lookup(u"0"_s, path, 512), retained);
        QCOMPARE(cache.statistics()[u"completedLruBytes"_s].toInteger(), capacity * bytes);
        cache.reset();
        QCOMPARE(cache.statistics()[u"completedLruBytes"_s].toInteger(), 0);
        QVERIFY(!cache.lookup(u"0"_s, path, 512));
        QCOMPARE(retained->image.pixelColor(256, 256), QColor(Qt::blue));
    }
    void cancellationAndClose()
    {
        QTemporaryDir directory;
        QImage image(1024, 1024, QImage::Format_RGBA8888);
        image.fill(Qt::blue);
        const auto path = directory.filePath(u"\u5c01\u9762 with spaces.png"_s);
        QVERIFY(image.save(path));
        CoverCache cache;
        QSignalSpy spy(&cache, &CoverCache::ready);
        for (int i = 0; i < 99; ++i)
            QVERIFY(cache.request(QString::number(i), path, 256));
        QVERIFY(!cache.request(u"overflow"_s, path, 256));
        cache.reset();
        for (int i = 0; i < 30; ++i) {
            QVERIFY(cache.request(QString::number(i), path, 1024));
            cache.cancel(QString::number(i));
        }
        QVERIFY(cache.request(u"final"_s, path, 1024));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, AsyncDeadlineMilliseconds);
        const auto final = qvariant_cast<CoverSnapshot>(spy.first().first());
        QCOMPARE(final->identity, u"final"_s);
        QCOMPARE(final->generation, cache.generation());
        QCOMPARE(final->image.pixelColor(512, 512), QColor(Qt::blue));
        QTRY_COMPARE(cache.statistics()[u"active"_s].toInt(), 0);
        QVERIFY(cache.statistics()[u"stale"_s].toInt() > 0);
        for (int i = 0; i < 10; ++i) {
            auto closing = std::make_unique<CoverCache>();
            QVERIFY(closing->request(u"closing"_s, path, 1024));
            closing.reset();
        }
        QVERIFY(cache.request(u"after-close"_s, path, 256));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 2, AsyncDeadlineMilliseconds);
    }
    void imageAllocationPolicy_data()
    {
        QTest::addColumn<QByteArray>("format");
        QTest::newRow("png") << "png"_ba;
        QTest::newRow("jpeg-scaled") << "jpeg"_ba;
        QTest::newRow("webp") << "webp"_ba;
    }
    void imageAllocationPolicy()
    {
        QFETCH(QByteArray, format);
        QTemporaryDir directory;
        QImage image(2049, 2048, QImage::Format_RGB32);
        image.fill(Qt::red);
        const auto path = directory.filePath(u"cover."_s + QString::fromUtf8(format));
        QVERIFY(image.save(path, format.constData()));
        CoverCache cache;
        QSignalSpy spy(&cache, &CoverCache::ready);
        QVERIFY(cache.request(u"cover"_s, path, 256));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, AsyncDeadlineMilliseconds);
        const auto pixels = qvariant_cast<CoverSnapshot>(spy.first().first());
        const auto limit = QImageReader::allocationLimit();
        bool configured = false;
        const auto requestedLimit =
                qEnvironmentVariableIntValue("QT_IMAGEIO_MAXALLOC", &configured);
        if (configured && requestedLimit >= 0)
            QCOMPARE(limit, requestedLimit);
        const auto reportedLimit = cache.statistics().value(u"imageAllocationLimitBytes"_s);
        if (limit == 0)
            QVERIFY(reportedLimit.isNull());
        else
            QCOMPARE(reportedLimit.toInteger(), qint64(limit) * 1024 * 1024);
        // JPEG scales during decode; PNG/WebP allocate the source image first.
        const bool allowed = format == "jpeg"_ba || limit == 0
                || image.sizeInBytes() <= qint64(limit) * 1024 * 1024;
        QCOMPARE(pixels->error.isEmpty(), allowed);
        QCOMPARE(pixels->sourceSize, image.size());
        QCOMPARE(pixels->image.size(), QSize(256, 256));
        if (allowed) {
            QVERIFY(pixels->image.pixelColor(128, 128).red() > 250);
            QVERIFY(pixels->readerReturnedBytes > 0);
        } else {
            QCOMPARE(pixels->readerReturnedBytes, 0);
        }
    }
    void missingCovers()
    {
        QTemporaryDir directory;
        CoverCache cache;
        QSignalSpy spy(&cache, &CoverCache::ready);
        QVERIFY(cache.request(u"missing"_s, directory.filePath(u"missing.png"_s), 256,
                              u"Missing album"_s));
        QVERIFY(cache.request(u"absent"_s, u""_s, 256, u"No cover album"_s));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 2, AsyncDeadlineMilliseconds);
        for (const auto &call : spy) {
            const auto p = qvariant_cast<CoverSnapshot>(call.first());
            QVERIFY(!p->error.isEmpty());
            QCOMPARE(p->image.pixelColor(0, 0).alpha(), 0);
            QCOMPARE(p->image.pixelColor(128, 128).alpha(), 255);
        }
    }
    void closeInsidePublication()
    {
        auto *cache = new CoverCache;
        QPointer<CoverCache> guard(cache);
        connect(cache, &CoverCache::ready, this, [cache] {
            delete cache;
        });
        QVERIFY(cache->request(u"missing"_s, u""_s, 256));
        QTRY_VERIFY(guard.isNull());
    }
    void taggedColorConversion()
    {
        QTemporaryDir directory;
        QImage image(512, 256, QImage::Format_RGB32);
        image.fill(QColor(180, 100, 40));
        image.setColorSpace(QColorSpace::DisplayP3);
        const auto expected = image.convertedToColorSpace(QColorSpace::SRgb).pixelColor(256, 128);
        const auto path = directory.filePath(u"tagged.png"_s);
        QVERIFY(image.save(path));
        CoverCache cache;
        QSignalSpy spy(&cache, &CoverCache::ready);
        QVERIFY(cache.request(u"tagged"_s, path, 256));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, AsyncDeadlineMilliseconds);
        const auto p = qvariant_cast<CoverSnapshot>(spy.first().first());
        QCOMPARE(p->image.pixelColor(128, 128), expected);
    }
    void smallSourceDoesNotUpsampleTwice()
    {
        QTemporaryDir directory;
        QImage image(64, 128, QImage::Format_RGBA8888);
        image.fill(Qt::blue);
        const auto path = directory.filePath(u"small.png"_s);
        QVERIFY(image.save(path));
        CoverCache cache;
        QSignalSpy spy(&cache, &CoverCache::ready);
        QVERIFY(cache.request(u"small"_s, path, 1024));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, AsyncDeadlineMilliseconds);
        const auto p = qvariant_cast<CoverSnapshot>(spy.first().first());
        QCOMPARE(p->readerReturnedBytes, 64 * 128 * 4);
        QCOMPARE(p->image.size(), QSize(1024, 1024));
        QCOMPARE(p->image.pixelColor(512, 512), QColor(Qt::blue));
    }
    void atlasOwnershipAndBudget()
    {
        AtlasAllocator atlas(2048, 432);
        QCOMPARE(atlas.pages(), 7);
        QList<AtlasLease> leases;
        for (int i = 0; i < 432; ++i) {
            const auto id = QString::number(i);
            auto lease = atlas.acquire(id);
            QVERIFY(lease.lease);
            QVERIFY(atlas.publish(lease, pixels(id)));
            leases.append(lease);
        }
        QVERIFY(!atlas.acquire(u"overflow"_s).lease);
        QCOMPARE(atlas.retainedBytes(), 432 * AtlasUploadQueue::tileBytes);
        QCOMPARE(atlas.uploads(0, false).size(), 8);
        QCOMPARE(atlas.uploads(0, true).size(), 56);
        QVERIFY(atlas.uploads(0, false).isEmpty());
        QCOMPARE(atlas.replay(0).size(), 64);
        const auto previous = leases.first();
        atlas.release(previous);
        const auto next = atlas.acquire(u"next"_s);
        QCOMPARE(next.slot, previous.slot);
        QVERIFY(next.lease > previous.lease);
        QVERIFY(!atlas.publish(previous, pixels(previous.identity)));
        QVERIFY(atlas.publish(next, pixels(u"next"_s)));
        AtlasUploadQueue queue;
        queue.reset(atlas.pageSize(), atlas.generation());
        queue.enqueue(atlas.uploads(0, false));
        QVERIFY(!queue.ready(next.slot, next.lease));
        QCOMPARE(queue.take(false).size(), 1);
        QVERIFY(queue.ready(next.slot, next.lease));
        queue.expect(next.slot, next.lease + 1);
        QVERIFY(!queue.ready(next.slot, next.lease + 1));
        queue.enqueue(
                { { atlas.generation(), next.lease, 999, next.slot, pixels(u"next"_s)->image } });
        QVERIFY(queue.take(false).isEmpty());
        atlas.reset();
        QVERIFY(!atlas.publish(next, pixels(u"next"_s)));
        QCOMPARE(atlas.retainedBytes(), 0);
        AtlasAllocator small(512, 432);
        QCOMPARE(small.pages(), 108);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, AtlasAllocator(255));
    }
    void missingCoverTitleRevision()
    {
        CoverCache cache;
        QSignalSpy ready(&cache, &CoverCache::ready);
        QVERIFY(cache.request(u"a"_s, { }, 256, u"Old title"_s));
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, AsyncDeadlineMilliseconds);
        const auto old = cache.lookup(u"a"_s, { }, 256, u"Old title"_s);
        QVERIFY(old);
        QVERIFY(!cache.lookup(u"a"_s, { }, 256, u"New title"_s));
        QVERIFY(cache.request(u"a"_s, { }, 256, u"New title"_s));
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 2, AsyncDeadlineMilliseconds);
        const auto changed = cache.lookup(u"a"_s, { }, 256, u"New title"_s);
        QVERIFY(changed && changed != old);
        QCOMPARE(cache.lookup(u"a"_s, { }, 256, u"Old title"_s), old);
    }
    void corruptCoverFallbackTracksTitleAndValidArtworkIgnoresIt()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto brokenPath = directory.filePath(u"broken.png"_s);
        QFile broken(brokenPath);
        QVERIFY(broken.open(QIODevice::WriteOnly));
        QCOMPARE(broken.write("broken"_ba), qint64(6));
        broken.close();
        CoverCache cache;
        QSignalSpy ready(&cache, &CoverCache::ready);
        QVERIFY(cache.request(u"broken"_s, brokenPath, 256, u"Old title"_s));
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, AsyncDeadlineMilliseconds);
        const auto old = cache.lookup(u"broken"_s, brokenPath, 256, u"Old title"_s);
        QVERIFY(old && !old->error.isEmpty());
        QCOMPARE(old->fallbackTitle, u"Old title"_s);
        QVERIFY(!cache.lookup(u"broken"_s, brokenPath, 256, u"New title"_s));
        QVERIFY(cache.request(u"broken"_s, brokenPath, 256, u"New title"_s));
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 2, AsyncDeadlineMilliseconds);
        const auto changed = cache.lookup(u"broken"_s, brokenPath, 256, u"New title"_s);
        QVERIFY(changed && changed != old);
        QCOMPARE(changed->fallbackTitle, u"New title"_s);
        QImage image(16, 16, QImage::Format_RGB32);
        image.fill(Qt::red);
        const auto validPath = directory.filePath(u"valid.png"_s);
        QVERIFY(image.save(validPath));
        QVERIFY(cache.request(u"valid"_s, validPath, 256, u"Original title"_s));
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 3, AsyncDeadlineMilliseconds);
        const auto valid = cache.lookup(u"valid"_s, validPath, 256, u"Original title"_s);
        QVERIFY(valid && valid->error.isEmpty());
        const auto decodes = cache.statistics().value(u"decodeStarts"_s);
        QCOMPARE(cache.lookup(u"valid"_s, validPath, 256, u"Renamed title"_s), valid);
        QVERIFY(cache.request(u"valid"_s, validPath, 256, u"Renamed title"_s));
        QCOMPARE(cache.statistics().value(u"decodeStarts"_s), decodes);
    }
    void failedUpgradePreservesDecodedArtwork()
    {
        SelectedCoverPool pool;
        const auto valid = pixels(u"album"_s);
        const auto lease = pool.acquire(u"album"_s, valid);
        auto failed = std::make_shared<CoverPixels>(*pixels(u"album"_s, 1024));
        failed->error = u"Decoder failed"_s;
        QVERIFY(!pool.upgrade(lease, failed));
        QCOMPARE(lease->pixels, valid);
        QVERIFY(pool.upgrade(lease, pixels(u"album"_s, 1024)));
        QCOMPARE(lease->pixels->resolution, 1024);
    }
    void targetedContentInvalidation()
    {
        QTemporaryDir directory;
        QImage image(32, 32, QImage::Format_RGB32);
        image.fill(Qt::red);
        const auto path = directory.filePath(u"same-path.png"_s);
        QVERIFY(image.save(path));
        CoverCache cache;
        QSignalSpy ready(&cache, &CoverCache::ready);
        QVERIFY(cache.request(u"a"_s, path, 256));
        QVERIFY(cache.request(u"b"_s, path, 256));
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 2, AsyncDeadlineMilliseconds);
        const auto stable = cache.lookup(u"b"_s, path, 256);
        const auto old = cache.lookup(u"a"_s, path, 256);
        SelectedCoverPool owners;
        const auto selected = owners.acquire(u"a"_s, old);
        const auto returning = owners.fork(selected);
        image.fill(Qt::blue);
        QVERIFY(image.save(path));
        cache.invalidate(u"a"_s);
        QVERIFY(!cache.lookup(u"a"_s, path, 256));
        QCOMPARE(cache.lookup(u"b"_s, path, 256), stable);
        QVERIFY(cache.request(u"a"_s, path, 256));
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 3, AsyncDeadlineMilliseconds);
        const auto changed = cache.lookup(u"a"_s, path, 256);
        QVERIFY(changed != old);
        QVERIFY(owners.upgrade(selected, changed));
        QCOMPARE(returning->pixels, old);
        QCOMPARE(returning->pixels->image.pixelColor(128, 128), QColor(Qt::red));
        QCOMPARE(selected->pixels->image.pixelColor(128, 128), QColor(Qt::blue));
        const auto decodes = cache.statistics().value(u"decodeStarts"_s);
        for (int i = 0; i < 20; ++i)
            QVERIFY(cache.request(u"b"_s, path, 256));
        QCOMPARE(cache.statistics()[u"decodeStarts"_s], decodes);
    }
    void snapshotPreservesUnchangedAtlas()
    {
        QTemporaryDir directory;
        LibrarySnapshot library;
        LibraryColumn column;
        QImage image(32, 32, QImage::Format_RGB32);
        image.fill(Qt::red);
        for (const auto &id : { "a", "b" }) {
            const auto path = directory.filePath(QString::fromUtf8(id) + ".png"_L1);
            QVERIFY(image.save(path));
            Album album;
            album.id = QString::fromUtf8(id);
            album.title = QString::fromUtf8(id);
            album.coverUrl = QUrl::fromLocalFile(path);
            library.albums.append(album);
            column.albumIds.append(QString::fromUtf8(id));
        }
        MotionController motion;
        SelectionController selection(&motion);
        selection.setColumns({ column }, std::make_shared<const LibrarySnapshot>(library));
        SceneAdapter adapter;
        adapter.setSelection(&selection);
        QQuickWindow window;
        HiddenSceneItem item(window.contentItem());
        item.setSize({ 960, 640 });
        item.setAdapter(&adapter);
        item.publishNow();
        QTRY_VERIFY_WITH_TIMEOUT(
                (item.publishNow(),
                 item.coverStatistics()[u"ready"_s].toInt() == 2
                         && item.coverStatistics()[u"selectedDetailed"_s].toBool()),
                AsyncDeadlineMilliseconds);
        item.setPreparing(true);
        item.publishNow();
        QSignalSpy prepared(&item, &SceneItem::scenePrepared);
        const auto token = item.input().preparationToken;
        const auto submission = item.submission();
        item.prepared(token - 1, submission, true);
        item.prepared(token, submission - 1, true);
        item.prepared(token, submission, false);
        QCoreApplication::sendPostedEvents();
        QCOMPARE(prepared.size(), 0);
        item.prepared(token, submission, true);
        QTRY_COMPARE(prepared.size(), 1);
        item.setPreparing(false);
        item.publishNow();
        const auto before = item.input();
        const auto decodes =
                item.coverStatistics().value(u"cache"_s).toObject().value(u"decodeStarts"_s);
        adapter.recordInput();
        item.publishNow();
        QVERIFY(item.input().entities.isSharedWith(before.entities));
        QVERIFY(item.input().entityIds.isSharedWith(before.entityIds));
        QVERIFY(item.input().coverTiles.isSharedWith(before.coverTiles));
        QVERIFY(item.input().coverLeases.isSharedWith(before.coverLeases));
        QVERIFY(item.input().atlasUploads.isSharedWith(before.atlasUploads));
        selection.setLift(.41);
        item.publishNow();
        QVERIFY(item.input().entities.last() != before.entities.last());
        QVERIFY(item.input().entityIds.isSharedWith(before.entityIds));
        QVERIFY(item.input().coverTiles.isSharedWith(before.coverTiles));
        QVERIFY(item.input().coverLeases.isSharedWith(before.coverLeases));
        QVERIFY(item.input().fallbackCoverTiles.isSharedWith(before.fallbackCoverTiles));
        QVERIFY(item.input().fallbackCoverLeases.isSharedWith(before.fallbackCoverLeases));
        // A different song in the same album does not alter cover identity or pixels.
        library.albums[0].tracks.append(Track{ });
        selection.setColumns({ column }, std::make_shared<const LibrarySnapshot>(library));
        item.publishNow();
        QCOMPARE(item.input().atlasGeneration, before.atlasGeneration);
        QCOMPARE(item.input().coverLeases, before.coverLeases);
        QCOMPARE(item.coverStatistics()[u"cache"_s].toObject()[u"decodeStarts"_s], decodes);
        // Change only a's source revision; b's slot and pixel serial remain stable.
        const auto changedPath = directory.filePath(u"a-new.png"_s);
        image.fill(Qt::blue);
        QVERIFY(image.save(changedPath));
        library.albums[0].coverUrl = QUrl::fromLocalFile(changedPath);
        selection.setColumns({ column }, std::make_shared<const LibrarySnapshot>(library));
        item.publishNow();
        QTRY_VERIFY_WITH_TIMEOUT(
                (item.publishNow(), item.coverStatistics()[u"ready"_s].toInt() == 2),
                AsyncDeadlineMilliseconds);
        const auto after = item.input();
        QCOMPARE(after.atlasGeneration, before.atlasGeneration);
        int changed = 0, stable = 0;
        for (qsizetype i = 0; i < after.coverLeases.size(); ++i) {
            if (before.coverLeases[i] == after.coverLeases[i])
                ++stable;
            else
                ++changed;
        }
        QVERIFY(changed > 0);
        QVERIFY(stable > 0);
        for (const auto &upload : before.atlasUploads) {
            if (upload.slot >= 484)
                continue;
            for (const auto &next : after.atlasUploads)
                if (next.slot == upload.slot && next.lease == upload.lease)
                    QCOMPARE(next.serial, upload.serial);
        }
        QTRY_VERIFY_WITH_TIMEOUT(
                (item.publishNow(), item.coverStatistics()[u"selectedDetailed"_s].toBool()),
                AsyncDeadlineMilliseconds);
        const auto resolution = item.coverStatistics()[u"owners"_s]
                                        .toArray()
                                        .first()
                                        .toObject()[u"resolution"_s]
                                        .toInt();
        item.setSize({ 1920, 1280 });
        item.publishNow();
        QTRY_VERIFY_WITH_TIMEOUT(
                (item.publishNow(), item.coverStatistics()[u"selectedDetailed"_s].toBool()),
                AsyncDeadlineMilliseconds);
        QVERIFY(item.coverStatistics()[u"owners"_s]
                        .toArray()
                        .first()
                        .toObject()[u"resolution"_s]
                        .toInt()
                > resolution);
        QVERIFY(item.input().atlasGeneration != after.atlasGeneration);
    }
    void selectedAndReturning()
    {
        SelectedCoverPool pool;
        const auto low = pixels(u"a"_s);
        auto selected = pool.acquire(u"a"_s, low);
        auto returning = pool.fork(selected);
        QVERIFY(selected && returning && selected->lease != returning->lease);
        QVERIFY(pool.upgrade(selected, pixels(u"a"_s, 1024)));
        QCOMPARE(returning->pixels, low);
        QVERIFY(!pool.upgrade(selected, pixels(u"b"_s, 1024)));
        QVERIFY(!pool.upgrade(selected, pixels(u"a"_s, 1024, 2)));
        auto third = pool.acquire(u"b"_s);
        auto fourth = pool.acquire(u"c"_s);
        QVERIFY(!pool.acquire(u"overflow"_s));
        pool.reset();
        QVERIFY(!pool.upgrade(selected, pixels(u"a"_s, 1024)));
        QVERIFY(!pool.acquire(u"still-pinned"_s));
        fourth.reset();
        QVERIFY(pool.acquire(u"released"_s));
    }
};
QTEST_MAIN(CoverCacheTest)
#include "CoverCacheTest.moc"
