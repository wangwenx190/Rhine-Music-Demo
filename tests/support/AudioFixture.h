// Generates bounded anonymous RIFF/WAVE input for public Qt Multimedia tests.
// Header fields describe mono signed 16-bit PCM; production playback never implements PCM handling.

#pragma once
#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#include <cmath>
namespace yalahoo::test {
namespace audioFixture {
// RIFF layout and low-amplitude tone parameters are independent anonymous test input.
inline constexpr auto SampleBits = 16;
inline constexpr auto SamplesPerMillisecond = 16;
inline constexpr auto FormatPayloadBytes = 16;
inline constexpr auto ChunkTagBytes = 4;
inline constexpr auto RiffPayloadHeaderBytes = 36;
inline constexpr auto WaveFormatTagBytes = 8;
inline constexpr auto PcmFormat = 1;
inline constexpr auto MonoChannels = 1;
inline constexpr auto SampleRateHertz = 16000;
inline constexpr auto ByteRate = 32000;
inline constexpr auto SampleBytes = 2;
inline constexpr auto Amplitude = 1000;
inline constexpr auto Pi = 3.141592653589793;
inline constexpr auto ToneHertz = 220;
}

inline QByteArray wave(int milliseconds)
{
    // Locally generated RIFF test input only; production never handles PCM.
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    const int samples = milliseconds * audioFixture::SamplesPerMillisecond;
    out.writeRawData("RIFF", audioFixture::ChunkTagBytes);
    out << quint32(audioFixture::RiffPayloadHeaderBytes + samples * audioFixture::SampleBytes);
    out.writeRawData("WAVEfmt ", audioFixture::WaveFormatTagBytes);
    out << quint32(audioFixture::FormatPayloadBytes) << quint16(audioFixture::PcmFormat)
        << quint16(audioFixture::MonoChannels) << quint32(audioFixture::SampleRateHertz)
        << quint32(audioFixture::ByteRate) << quint16(audioFixture::SampleBytes)
        << quint16(audioFixture::SampleBits);
    out.writeRawData("data", audioFixture::ChunkTagBytes);
    out << quint32(samples * audioFixture::SampleBytes);
    for (int i = 0; i < samples; ++i)
        out << qint16(audioFixture::Amplitude
                      * std::sin(i * 2 * audioFixture::Pi * audioFixture::ToneHertz
                                 / audioFixture::SampleRateHertz));
    return bytes;
}
}
