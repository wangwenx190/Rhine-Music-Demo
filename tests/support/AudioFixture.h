#pragma once
#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#include <cmath>
namespace yalahoo::test {
inline QByteArray wave(int milliseconds)
{
    // Locally generated RIFF test input only; production never handles PCM.
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    const int samples = milliseconds * 16;
    out.writeRawData("RIFF", 4);
    out << quint32(36 + samples * 2);
    out.writeRawData("WAVEfmt ", 8);
    out << quint32(16) << quint16(1) << quint16(1) << quint32(16000) << quint32(32000) << quint16(2)
        << quint16(16);
    out.writeRawData("data", 4);
    out << quint32(samples * 2);
    for (int i = 0; i < samples; ++i)
        out << qint16(1000 * std::sin(i * 2 * 3.141592653589793 * 220 / 16000));
    return bytes;
}
}
