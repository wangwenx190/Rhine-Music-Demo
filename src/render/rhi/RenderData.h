// Defines the production renderer's immutable resource dimensions and binary strides.
// Values match bundled lookup payloads, atlas tiles and the shader vertex/uniform ABI.

#pragma once

namespace yalahoo::rhi::renderData {
inline constexpr int CoverTilePixels = 256;
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
}
