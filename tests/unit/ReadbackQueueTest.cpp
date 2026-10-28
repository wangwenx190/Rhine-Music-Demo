// Holds controlled backend completions to check the production capture queue's bounded
// continuation and retirement independently of the platform's synchronous/offscreen behavior.
#include "ReadbackQueue_p.h"
#include <QTest>

using namespace Qt::StringLiterals;

namespace yalahoo::rhi {
class ReadbackQueueTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ReadbackQueueTest)
public:
    ReadbackQueueTest() = default;
private Q_SLOTS:
    void pendingCaptureContinuesUntilCompletionOrItsFrameBound()
    {
        ReadbackQueue queue;
        QVERIFY(!queue.drain());
        auto pending = std::make_shared<ReadbackQueue::Pending>();
        pending->remaining = 1;
        pending->drainFrames = 3;
        pending->readbackOwner.fill(0);
        auto &result = pending->results[0];
        result.format = QRhiTexture::RGBA8;
        result.pixelSize = { 1, 1 };
        result.data = QByteArray::fromHex("ff0000ff"_ba);
        int delivered = 0;
        pending->deliver = [&](Capture capture) {
            ++delivered;
            QCOMPARE(capture.color.size(), QSize(1, 1));
        };
        queue.pending.push_back(pending);
        // These are clear-only scene frames: no new capture or scene work is submitted.
        QVERIFY(queue.drain());
        QVERIFY(queue.drain());
        QVERIFY(queue.drain());
        QVERIFY(!queue.drain());
        QCOMPARE(delivered, 0);
        ReadbackQueue::completeReadback(pending, 0);
        QCOMPARE(delivered, 1);
        QVERIFY(!queue.drain());
        queue.retire();
        QVERIFY(queue.pending.empty());
        QVERIFY(queue.hasCapacity());
    }
    void completionStopsContinuationWithoutUsingTheRemainingBudget()
    {
        ReadbackQueue queue;
        auto pending = std::make_shared<ReadbackQueue::Pending>();
        pending->remaining = 1;
        pending->drainFrames = 3;
        queue.pending.push_back(pending);
        QVERIFY(queue.drain());
        pending->remaining = 0;
        QVERIFY(!queue.drain());
        QCOMPARE(pending->drainFrames, 2);
        queue.retire();
        QVERIFY(queue.pending.empty());
    }
};
}
QTEST_GUILESS_MAIN(yalahoo::rhi::ReadbackQueueTest)
#include "ReadbackQueueTest.moc"
