// Defines the production renderer's immutable resource dimensions and binary strides.
// Values match bundled lookup payloads, atlas tiles and the shader vertex/uniform ABI.

#pragma once
#include "CoverPixels.h"

namespace yalahoo::rhi::renderData {
inline constexpr int CoverTilePixels = coverPixels::TileSide;
inline constexpr int MaximumCaseEntities = 436;
inline constexpr int EnvironmentWidth = 768;
inline constexpr int EnvironmentHeight = 1024;
inline constexpr int DfgSide = 16;
inline constexpr int LtcSide = 64;
inline constexpr int NoiseSide = 4;
inline constexpr int SmaaAreaWidth = 160;
inline constexpr int SmaaAreaHeight = 560;
inline constexpr int SmaaSearchWidth = 66;
inline constexpr int SmaaSearchHeight = 33;
inline constexpr int FloatBytes = 4;
inline constexpr int VectorBytes = 16;
inline constexpr int MatrixBytes = 64;
inline constexpr int VertexBytes = 32;
inline constexpr int InstanceBytes = 112;
inline constexpr int OutputUniformBytes = 32;
inline constexpr int PostUniformBytes = 1104;
inline constexpr auto CaseSurfaceCount = 3;
inline constexpr auto ExtraDrawUniforms = 6;
inline constexpr auto FullscreenTriangleVertices = 3;
inline constexpr auto PaperLeftMagnitude = 1.36f;
inline constexpr auto PaperRight = 1.7f;
inline constexpr auto PaperBottom = .32f;
inline constexpr auto PaperFront = .027f;
inline constexpr auto PaperTop = 3.38f;
}
