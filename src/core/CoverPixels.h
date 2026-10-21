// Defines the shared RGBA cover-tile payload consumed by decoding, atlas upload and probes.

#pragma once
#include "Units.h"
namespace yalahoo::coverPixels {
inline constexpr int TileSide = 256;
inline constexpr int TileBytes = TileSide * TileSide * units::RgbaBytesPerPixel;
}
