// Bounded binary cursor shared by geometry decoders.
#pragma once
#include "Require.h"
#include <QByteArray>
#include <QtEndian>
#include <cmath>
#include <cstring>
namespace yalahoo::assets::geometryDetail {
struct Reader
{
    const QByteArray &bytes;
    qsizetype offset = 0;
    qsizetype limit = bytes.size();
    QByteArray take(qsizetype size)
    {
        require(size >= 0 && offset >= 0 && offset <= limit && size <= limit - offset,
                "Truncated geometry");
        auto result = bytes.mid(offset, size);
        offset += size;
        return result;
    }
    quint32 u32()
    {
        auto data = take(4);
        return qFromLittleEndian<quint32>(data.constData());
    }
    quint16 u16()
    {
        auto data = take(2);
        return qFromLittleEndian<quint16>(data.constData());
    }
    float f32()
    {
        auto bits = u32();
        float value;
        std::memcpy(&value, &bits, 4);
        require(std::isfinite(value), "Nonfinite vertex");
        return value;
    }
};

}
