// Transfers owned readbacks to a GUI event-loop waiter. The shared state outlives queued
// completions; request identity and the guarded receiver reject late or destroyed work.
#pragma once
#include "RhiItem.h"
#include <QEventLoop>
#include <QPointer>
#include <QObject>
#include <memory>
#include <optional>
#include <utility>

namespace yalahoo::review {
struct CaptureMailbox final
{
    quint64 request = 0;
    QPointer<QEventLoop> loop;
    std::optional<rhi::Capture> capture;
    QString error;
};
inline void queueCapture(const std::shared_ptr<CaptureMailbox> &state,
                         const QPointer<QObject> &guard, rhi::Capture capture)
{
    if (!guard)
        return;
    QMetaObject::invokeMethod(
            guard,
            [state, capture = std::move(capture)]() mutable {
                if (state->loop && capture.request == state->request) {
                    state->capture = std::move(capture);
                    state->loop->quit();
                }
            },
            Qt::QueuedConnection);
}
}
