#include "AtlasUpload.h"
#include <QtTest>
using namespace yalahoo::rhi;
class AtlasUploadTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AtlasUploadTest)
public:
    AtlasUploadTest() = default;

private Q_SLOTS:
    void leaseInvalidationRetainsIndependentSlotHistory()
    {
        AtlasUploadQueue queue;
        queue.reset({ 512, 256 }, 9);
        QImage pixels(256, 256, QImage::Format_RGBA8888_Premultiplied);
        pixels.fill(Qt::red);
        queue.enqueue({ { 9, 2, 10, 0, pixels, true }, { 9, 5, 20, 1, pixels, true } });
        QCOMPARE(queue.take(false).size(), 2);
        QVERIFY(queue.ready(0, 2));
        QVERIFY(queue.ready(1, 5));
        queue.expect(0, 3);
        QVERIFY(!queue.ready(0, 2));
        QVERIFY(!queue.ready(0, 3));
        QVERIFY(queue.ready(1, 5));
        // A stale completion must not overwrite the new expectation, even with a later serial.
        queue.enqueue({ { 9, 2, 11, 0, pixels, true } });
        QCOMPARE(queue.pending(), 0);
        // A new lease may start its own serial after expect() reset the previous one.
        queue.enqueue({ { 9, 3, 1, 0, pixels, true } });
        queue.expect(0, 2);
        QCOMPARE(queue.take(false).size(), 1);
        QVERIFY(queue.ready(0, 3));
        QVERIFY(queue.ready(1, 5));
        queue.reset({ 512, 256 }, 10);
        QVERIFY(!queue.ready(0, 3));
        QVERIFY(!queue.ready(1, 5));
    }
    void highResolutionRegionReadiness()
    {
        AtlasUploadQueue queue;
        queue.reset({ 5632, 6656 }, 7);
        const QRect region(0, 5632, 1024, 1024);
        QImage pixels(256, 256, QImage::Format_RGBA8888_Premultiplied);
        pixels.fill(Qt::red);
        QVector<AtlasUpload> chunks;
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x) {
                const int slot = (22 + y) * 22 + x;
                queue.expect(slot, 3);
                chunks.append({ 7, 3, 1, slot, pixels, true });
            }
        queue.enqueue(chunks);
        QCOMPARE(queue.take(false).size(), 8);
        QVERIFY(!queue.ready(region, 3));
        QCOMPARE(queue.take(false).size(), 8);
        QVERIFY(queue.ready(region, 3));
        QVERIFY(!queue.ready(region, 2));
        queue.expect(22 * 22, 4);
        QVERIFY(!queue.ready(region, 3));
    }
    void mipPayloadParticipatesInBudget()
    {
        AtlasUploadQueue queue;
        queue.reset({ 2048, 2048 }, 1);
        QImage image(256, 256, QImage::Format_RGBA8888_Premultiplied);
        image.fill(Qt::white);
        QList<QImage> mips;
        for (int side = 128; side >= 1; side /= 2) {
            QImage mip(side, side, image.format());
            mip.fill(Qt::white);
            mips.append(mip);
        }
        for (int slot = 0; slot < 12; ++slot)
            queue.enqueue({ { 1, 1, 1, slot, image, true, mips } });
        const auto batch = queue.take(false);
        qint64 bytes = 0;
        for (const auto &upload : batch)
            bytes += AtlasUploadQueue::payloadBytes(upload);
        QVERIFY(bytes <= AtlasUploadQueue::steadyBudget);
        QVERIFY(bytes + AtlasUploadQueue::payloadBytes(batch.first())
                > AtlasUploadQueue::steadyBudget);
        QVERIFY(queue.pending() > 0);
        queue.enqueue({ { 1, 1, 1, 0, image, true, mips } });
        QCOMPARE(queue.pending(), 12 - batch.size());
    }
    void ownedSnapshot()
    {
        AtlasUploadQueue queue;
        queue.reset({ 256, 256 }, 1);
        QByteArray backing(256 * 256 * 4, '\0');
        QImage external(reinterpret_cast<uchar *>(backing.data()), 256, 256,
                        QImage::Format_RGBA8888_Premultiplied);
        external.fill(Qt::red);
        queue.enqueue({ { 1, 1, 1, 0, external } });
        // Modify the external storage without Qt's detach: the queued image must own its bytes.
        backing.fill('\0');
        const auto batch = queue.take(false);
        QCOMPARE(batch.size(), 1);
        QCOMPARE(batch[0].pixels.pixelColor(0, 0), QColor(Qt::red));
        QCOMPARE(batch[0].pixels.format(), QImage::Format_RGBA8888_Premultiplied);
    }
    void budgetsAndDedupe()
    {
        AtlasUploadQueue queue;
        queue.reset({ 2048, 4096 }, 1);
        QImage pixels(256, 256, QImage::Format_RGBA8888);
        pixels.fill(Qt::red);
        QVector<AtlasUpload> jobs;
        for (int i = 0; i < 128; ++i)
            jobs.append({ 1, 1, 1, i, pixels });
        queue.enqueue(jobs);
        QCOMPARE(queue.take(true).size(), 64);
        QCOMPARE(queue.take(true).size(), 64);
        QVERIFY(queue.take(true).isEmpty());
        queue.enqueue(jobs);
        QVERIFY(queue.take(false).isEmpty());
        for (auto &u : jobs)
            u.serial = 2;
        queue.enqueue(jobs);
        QCOMPARE(queue.take(false).size(), 8);
        QCOMPARE(queue.pending(), 120);
    }
    void replacementAndGeneration()
    {
        AtlasUploadQueue queue;
        queue.reset({ 2048, 2048 }, 1);
        QImage pixels(256, 256, QImage::Format_RGBA8888);
        pixels.fill(Qt::red);
        queue.enqueue({ { 1, 1, 1, 0, pixels } });
        pixels.fill(Qt::blue);
        queue.enqueue({ { 1, 2, 2, 0, pixels } });
        const auto latest = queue.take(false);
        QCOMPARE(latest.size(), 1);
        QCOMPARE(latest[0].lease, quint64(2));
        QCOMPARE(latest[0].pixels.pixelColor(0, 0), QColor(Qt::blue));
        queue.enqueue({ { 1, 1, 3, 0, pixels } });
        QVERIFY(queue.take(false).isEmpty());
        queue.enqueue({ { 1, 1, 1, 0, pixels } });
        QVERIFY(queue.take(false).isEmpty());
        queue.reset({ 2048, 2048 }, 2);
        queue.enqueue({ { 1, 3, 10, 0, pixels }, { 2, 0, 1, 0, pixels }, { 2, 1, 1, 64, pixels } });
        QCOMPARE(queue.rejected(), quint64(4));
        QVERIFY(queue.take(false).isEmpty());
        queue.enqueue({ { 2, 1, 1, 0, pixels } });
        QCOMPARE(queue.take(false).size(), 1);
    }
};
QTEST_GUILESS_MAIN(AtlasUploadTest)
#include "AtlasUploadTest.moc"
