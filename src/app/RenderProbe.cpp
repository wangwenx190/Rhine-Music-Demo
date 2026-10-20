#include "RenderProbe.h"
namespace {
// Anonymous tile colors make incremental texture changes visible without loading a private cover.
constexpr auto AtlasSidePixels = 2048;
constexpr auto TileSidePixels = 256;
constexpr auto TilesPerRow = 8;
constexpr auto RgbaChannels = 4;
constexpr auto BaseRed = 40;
constexpr auto RedStepPerTile = 3;
constexpr auto BaseGreen = 180;
constexpr auto BaseBlue = 70;
constexpr auto UpdatedRed = 230;
constexpr auto UpdatedGreen = 30;
constexpr auto UpdatedBlue = 90;
}

void RenderProbeInstances::setPositions(const QVariantList &positions)
{
    m_buffer.clear();
    m_count = int(positions.size());
    for (int i = 0; i < m_count; ++i) {
        const auto p = positions[i].toList();
        const auto entry = calculateTableEntry({ p[0].toFloat(), p[1].toFloat(), p[2].toFloat() },
                                               { 1, 1, 1 }, { }, Qt::white, { float(i), 0, 0, 0 });
        m_buffer.append(reinterpret_cast<const char *>(&entry), sizeof(entry));
    }
    markDirty();
}
RenderProbeAtlas::RenderProbeAtlas(QQuick3DObject *parent)
    : QQuick3DTextureData(parent), m_pixels(pageBytes, '\0')
{
    setSize({ AtlasSidePixels, AtlasSidePixels });
    setFormat(RGBA8);
    setHasTransparency(false);
    // 64 deterministic tiles, no user music/artwork. Byte RGBA has no endian dependency.
    for (int y = 0; y < AtlasSidePixels; ++y)
        for (int x = 0; x < AtlasSidePixels; ++x) {
            const int i = (y * AtlasSidePixels + x) * RgbaChannels,
                      tile = (y / TileSidePixels) * TilesPerRow + x / TileSidePixels;
            m_pixels[i] = char(BaseRed + tile * RedStepPerTile);
            m_pixels[i + 1] = char(BaseGreen - tile * 2);
            m_pixels[i + 2] = char(BaseBlue + tile * 2);
            m_pixels[i + RedStepPerTile] = char(-1);
        }
    setPhase(0);
}
void RenderProbeAtlas::setPhase(int phase)
{
    if (phase == m_phase)
        return;
    m_phase = phase;
    for (int y = 0; y < TileSidePixels; ++y)
        for (int x = 0; x < TileSidePixels; ++x) {
            const int i = (y * AtlasSidePixels + x) * RgbaChannels;
            m_pixels[i] = char(phase ? UpdatedRed : BaseRed);
            m_pixels[i + 1] = char(phase ? UpdatedGreen : BaseGreen);
            m_pixels[i + 2] = char(phase ? UpdatedBlue : BaseBlue);
        }
    setTextureData(m_pixels);
    update();
    ++m_submissions;
}
