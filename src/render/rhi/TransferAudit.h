#pragma once
#include "RhiApi.h"
#include <QJsonArray>
#include <QJsonObject>
#if defined(Q_OS_WIN) && defined(YALAHOO_RHI_AUDIT)
#  include <private/qrhid3d11_p.h>
#endif

namespace yalahoo::rhi {
// Inspect the actual backend commands, not the caller's image payload or upload counters.
// Completed readbacks in the Review tool prove these commands executed successfully.
inline QJsonObject nativeTransfers(QRhi *api, QRhiCommandBuffer *cb, QRhiTexture *texture)
{
    QJsonObject result{ { "available", false } };
#if defined(Q_OS_WIN) && defined(YALAHOO_RHI_AUDIT)
    if (api->backend() == QRhi::D3D11) {
        const auto *native = static_cast<QD3D11Texture *>(texture)->textureResource();
        const auto *commands = static_cast<QD3D11CommandBuffer *>(cb);
        QJsonArray regions;
        qint64 bytes = 0;
        for (auto it = commands->commands.cbegin(); it != commands->commands.cend(); ++it) {
            const auto &command = *it;
            if (command.cmd != QD3D11CommandBuffer::Command::UpdateSubRes
                || command.args.updateSubRes.dst != native)
                continue;
            const auto &u = command.args.updateSubRes;
            require(u.hasDstBox && u.dstSubRes == 0, "Atlas native upload lost destination region");
            const auto &box = u.dstBox;
            const auto count = qint64(box.right - box.left) * (box.bottom - box.top) * 4;
            bytes += count;
            regions.append(QJsonObject{ { "left", int(box.left) },
                                        { "top", int(box.top) },
                                        { "right", int(box.right) },
                                        { "bottom", int(box.bottom) },
                                        { "rowPitch", int(u.srcRowPitch) },
                                        { "bytes", count } });
        }
        result = { { "available", true },
                   { "backend", "D3D11 UpdateSubresource" },
                   { "destinationBytes", bytes },
                   { "regions", regions },
                   { "physicalBusTrafficMeasured", false } };
    }
#else
    Q_UNUSED(api);
    Q_UNUSED(cb);
    Q_UNUSED(texture);
#endif
    return result;
}
}
