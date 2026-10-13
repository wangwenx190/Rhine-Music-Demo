#include "CoverCache.h"
#include "AtlasAllocator.h"
#include <QElapsedTimer>
#include <QColorSpace>
#include <QEventLoop>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QtTest>
using namespace yalahoo;
using namespace yalahoo::rhi;

class CoverCacheTest : public QObject
{
    Q_OBJECT
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
private slots:
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
            QVERIFY(large.save(directory.filePath(QString::number(i) + ".jpg")));
        }
        CoverCache cache;
        QSet<QString> ready;
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        bool valid = true, scaling = true;
        QElapsedTimer elapsed;
        elapsed.start();
        QList<qint64> gaps;
        qint64 lastTick = 0;
        QTimer input;
        input.setInterval(1);
        connect(&input, &QTimer::timeout, this, [&] {
            const auto now = elapsed.nsecsElapsed();
            gaps.append(now - lastTick);
            lastTick = now;
        });
        connect(&cache, &CoverCache::ready, this, [&](CoverSnapshot p) {
            ready.insert(p->identity);
            valid = valid && p->error.isEmpty() && p->image.size() == QSize(256, 256)
                    && p->image.pixelColor(0, 0).alpha() == 0;
            scaling = scaling && p->decoderScaling;
            if (ready.size() == 96)
                loop.quit();
        });
        input.start();
        for (int i = 0; i < 96; ++i) {
            const auto id = QString::number(i);
            QVERIFY(cache.request(id, directory.filePath(id + ".jpg"), 256));
            QVERIFY(cache.request(id, directory.filePath(id + ".jpg"), 256));
        }
        timeout.start(30000);
        loop.exec();
        QCOMPARE(ready.size(), 96);
        input.stop();
        const auto stats = cache.statistics();
        QVERIFY(valid);
        QVERIFY(scaling);
        QCOMPARE(stats["decodeStarts"].toInteger(), 96);
        QVERIFY(stats["peakActive"].toInt() <= 3);
        QVERIFY(stats["peakPending"].toInt() <= 96);
        QVERIFY(stats["completedLruBytes"].toInteger() <= CoverCache::cacheLimit);
        QCOMPARE(stats["reservedInFlightBytes"].toInteger(), 0);
        const auto cached = cache.lookup("95", directory.filePath("95.jpg"), 256);
        QVERIFY(cached);
        QVERIFY(cache.request("95", directory.filePath("95.jpg"), 256));
        QCOMPARE(cache.statistics()["decodeStarts"], stats["decodeStarts"]);
        QVERIFY(gaps.size() > 5);
        std::sort(gaps.begin(), gaps.end());
        QJsonObject report = stats;
        report["readyCount"] = ready.size();
        report["readyMs"] = elapsed.elapsed();
        report["eventTimerGapP95Ms"] = gaps[(gaps.size() - 1) * 95 / 100] / 1e6;
        report["eventTimerGapMaxMs"] = gaps.last() / 1e6;
        report["scope"] =
                "96 independent 2048-square JPEG sources; GUI event timer responsiveness, no scanout claim";
        report["passed"] = true;
        QSaveFile file(QString::fromUtf8(COVER_REPORT));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(report).toJson());
        QVERIFY(file.commit());
    }
    void cancellationAndClose()
    {
        QTemporaryDir directory;
        QImage image(1024, 1024, QImage::Format_RGBA8888);
        image.fill(Qt::blue);
        const auto path = directory.filePath(QStringLiteral("封面 with spaces.png"));
        QVERIFY(image.save(path));
        CoverCache cache;
        QSignalSpy spy(&cache, &CoverCache::ready);
        for (int i = 0; i < 99; ++i)
            QVERIFY(cache.request(QString::number(i), path, 256));
        QVERIFY(!cache.request("overflow", path, 256));
        cache.reset();
        for (int i = 0; i < 30; ++i) {
            QVERIFY(cache.request(QString::number(i), path, 1024));
            cache.cancel(QString::number(i));
        }
        QVERIFY(cache.request("final", path, 1024));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 10000);
        const auto final = qvariant_cast<CoverSnapshot>(spy.first().first());
        QCOMPARE(final->identity, QString("final"));
        QCOMPARE(final->generation, cache.generation());
        QCOMPARE(final->image.pixelColor(512, 512), QColor(Qt::blue));
        QTRY_COMPARE(cache.statistics()["active"].toInt(), 0);
        QVERIFY(cache.statistics()["stale"].toInt() > 0);
        for (int i = 0; i < 10; ++i) {
            auto closing = std::make_unique<CoverCache>();
            QVERIFY(closing->request("closing", path, 1024));
            closing.reset();
        }
        QVERIFY(cache.request("after-close", path, 256));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 2, 10000);
    }
    void missingAndExpansionGuard()
    {
        QTemporaryDir directory;
        QImage image(4096, 2048, QImage::Format_RGB32);
        image.fill(Qt::red);
        const auto path = directory.filePath("oversized.png");
        QVERIFY(image.save(path));
        CoverCache cache;
        QSignalSpy spy(&cache, &CoverCache::ready);
        QVERIFY(cache.request("oversized", path, 256));
        QVERIFY(cache.request("missing", directory.filePath("missing.png"), 256, "Missing album"));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 2, 10000);
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
        QVERIFY(cache->request("missing", "", 256));
        QTRY_VERIFY(guard.isNull());
    }
    void taggedColorConversion()
    {
        QTemporaryDir directory;
        QImage image(512, 256, QImage::Format_RGB32);
        image.fill(QColor(180, 100, 40));
        image.setColorSpace(QColorSpace::DisplayP3);
        const auto expected = image.convertedToColorSpace(QColorSpace::SRgb).pixelColor(256, 128);
        const auto path = directory.filePath("tagged.png");
        QVERIFY(image.save(path));
        CoverCache cache;
        QSignalSpy spy(&cache, &CoverCache::ready);
        QVERIFY(cache.request("tagged", path, 256));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 10000);
        const auto p = qvariant_cast<CoverSnapshot>(spy.first().first());
        QCOMPARE(p->image.pixelColor(128, 128), expected);
    }
    void smallSourceDoesNotUpsampleTwice()
    {
        QTemporaryDir directory;
        QImage image(64, 128, QImage::Format_RGBA8888);
        image.fill(Qt::blue);
        const auto path = directory.filePath("small.png");
        QVERIFY(image.save(path));
        CoverCache cache;
        QSignalSpy spy(&cache, &CoverCache::ready);
        QVERIFY(cache.request("small", path, 1024));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 10000);
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
        QVERIFY(!atlas.acquire("overflow").lease);
        QCOMPARE(atlas.retainedBytes(), 432 * AtlasUploadQueue::tileBytes);
        QCOMPARE(atlas.uploads(0, false).size(), 8);
        QCOMPARE(atlas.uploads(0, true).size(), 56);
        QVERIFY(atlas.uploads(0, false).isEmpty());
        QCOMPARE(atlas.replay(0).size(), 64);
        const auto previous = leases.first();
        atlas.release(previous);
        const auto next = atlas.acquire("next");
        QCOMPARE(next.slot, previous.slot);
        QVERIFY(next.lease > previous.lease);
        QVERIFY(!atlas.publish(previous, pixels(previous.identity)));
        QVERIFY(atlas.publish(next, pixels("next")));
        AtlasUploadQueue queue;
        queue.reset(atlas.pageSize(), atlas.generation());
        queue.enqueue(atlas.uploads(0, false));
        QVERIFY(!queue.ready(next.slot, next.lease));
        QCOMPARE(queue.take(false).size(), 1);
        QVERIFY(queue.ready(next.slot, next.lease));
        queue.expect(next.slot, next.lease + 1);
        QVERIFY(!queue.ready(next.slot, next.lease + 1));
        queue.enqueue(
                { { atlas.generation(), next.lease, 999, next.slot, pixels("next")->image } });
        QVERIFY(queue.take(false).isEmpty());
        atlas.reset();
        QVERIFY(!atlas.publish(next, pixels("next")));
        QCOMPARE(atlas.retainedBytes(), 0);
        AtlasAllocator small(512, 432);
        QCOMPARE(small.pages(), 108);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, AtlasAllocator(255));
    }
    void selectedAndReturning()
    {
        SelectedCoverPool pool;
        const auto low = pixels("a");
        auto selected = pool.acquire("a", low);
        auto returning = pool.fork(selected);
        QVERIFY(selected && returning && selected->lease != returning->lease);
        QVERIFY(pool.upgrade(selected, pixels("a", 1024)));
        QCOMPARE(returning->pixels, low);
        QVERIFY(!pool.upgrade(selected, pixels("b", 1024)));
        QVERIFY(!pool.upgrade(selected, pixels("a", 1024, 2)));
        auto third = pool.acquire("b");
        auto fourth = pool.acquire("c");
        QVERIFY(!pool.acquire("overflow"));
        pool.reset();
        QVERIFY(!pool.upgrade(selected, pixels("a", 1024)));
        QVERIFY(!pool.acquire("still-pinned"));
        fourth.reset();
        QVERIFY(pool.acquire("released"));
    }
};
QTEST_MAIN(CoverCacheTest)
#include "CoverCacheTest.moc"
