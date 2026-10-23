// QQuickRenderControl has void frame methods. Check QRhi recording state rather than a
// possibly stale commandBuffer(), and balance only frames that actually began recording.
#pragma once
#include <QScopeGuard>

namespace yalahoo::test {
template <typename Control, typename Operation>
bool renderControlFrame(Control &control, Operation operation)
{
    auto *api = control.rhi();
    if (!api || api->isDeviceLost() || api->isRecordingFrame())
        return false;
    control.beginFrame();
    if (!api->isRecordingFrame())
        return false;
    {
        const auto end = qScopeGuard([&] {
            control.endFrame();
        });
        auto *commands = control.commandBuffer();
        if (!commands)
            return false;
        operation(commands);
    }
    return !api->isDeviceLost();
}
}
