#include "AtlasUpload.h"
#include <QtTest>
using namespace yalahoo::rhi;
class AtlasUploadTest : public QObject
{
    Q_OBJECT
private slots:
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
