// Publishes bounded verification telemetry without embedding its collector in the product scene.
#include "SceneItemState_p.h"
#include <QMutexLocker>
#include <QQuickWindow>
using namespace Qt::StringLiterals;
namespace yalahoo::rhi {
namespace {
constexpr auto MaximumFeedbackSamples = 128;
}
struct SceneItem::Private::TelemetryMailbox final
{
    Q_DISABLE_COPY_MOVE(TelemetryMailbox)
    TelemetryMailbox() = default;
    QMutex mutex;
    QVector<QJsonObject> samples;
    bool queued = false;
    quint64 coalesced = 0;
};
void SceneItem::setTelemetryEnabled(bool enabled)
{
    if (d->m_telemetryEnabled == enabled)
        return;
    d->m_telemetryEnabled = enabled;
    d->configureTelemetry();
    d->schedule();
    Q_EMIT telemetryEnabledChanged();
}
void SceneItem::Private::publishTelemetry(const std::shared_ptr<Private::TelemetryMailbox> &mailbox,
                                          const QPointer<SceneItem> &guard, quint64 epoch)
{
    QVector<QJsonObject> batch;
    quint64 dropped = 0;
    {
        const QMutexLocker lock(&mailbox->mutex);
        batch.swap(mailbox->samples);
        dropped = mailbox->coalesced;
        mailbox->queued = false;
    }
    if (guard && epoch == guard->d->m_telemetryEpoch && guard->d->m_renderingActive
        && guard->telemetryEnabled())
        for (auto &sample : batch) {
            if (!guard || epoch != guard->d->m_telemetryEpoch || !guard->d->m_renderingActive
                || !guard->telemetryEnabled())
                break;
            sample[u"coalescedSamples"_s] = qint64(dropped);
            Q_EMIT guard->frameSubmitted(std::move(sample));
        }
}
void SceneItem::Private::queueTelemetry(const std::shared_ptr<Private::TelemetryMailbox> &mailbox,
                                        const QPointer<SceneItem> &guard, const quint64 &epoch,
                                        QJsonObject sample)
{
    const QMutexLocker lock(&mailbox->mutex);
    if (mailbox->samples.size() == MaximumFeedbackSamples) {
        mailbox->samples.removeFirst();
        ++mailbox->coalesced;
    }
    mailbox->samples.append(std::move(sample));
    if (mailbox->queued)
        return;
    mailbox->queued = true;
    if (guard)
        QMetaObject::invokeMethod(
                guard,
                [mailbox, guard, epoch] {
                    publishTelemetry(mailbox, guard, epoch);
                },
                Qt::QueuedConnection);
}
void SceneItem::Private::configureTelemetry()
{
    ++m_callbackRevision;
    const auto epoch = ++m_telemetryEpoch;
    q->profiled = { };
    if (m_telemetryEnabled && m_renderingActive) {

        auto mailbox = std::make_shared<TelemetryMailbox>();
        QPointer<SceneItem> guard(q);
        q->profiled = [mailbox, guard, epoch](QJsonObject sample) {
            return queueTelemetry(mailbox, guard, epoch, std::move(sample));
        };
    }
}
QJsonObject SceneItem::coverStatistics() const
{
    return d->m_sceneCovers.statistics();
}
bool SceneItem::telemetryEnabled() const
{
    return d->m_telemetryEnabled;
}
}
