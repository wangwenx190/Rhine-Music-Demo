// D3D11 exposes native command accounting. Other backends retain payload and GPU-content
// validation without claiming unavailable native-command proof.
#pragma once
#include "Require.h"
#include <QJsonObject>
#include <QSGRendererInterface>
using Qt::StringLiterals::operator""_s;

namespace yalahoo::review {
inline void checkAtlasTransfer(QSGRendererInterface::GraphicsApi backend,
                               const QJsonObject &metadata, qint64 expectedBytes)
{
    const auto native = metadata[u"nativeAtlasTransfers"_s].toObject();
    const bool nativeProof = native[u"available"_s].toBool();
    if (backend == QSGRendererInterface::Direct3D11)
        require(nativeProof, "Windows D3D11 native transfer proof unavailable");
    require((nativeProof ? native[u"destinationBytes"_s].toInteger()
                         : metadata[u"atlasUploadBytes"_s].toInteger())
                    == expectedBytes,
            "Initialization transfer bytes unavailable/over budget");
}
}
