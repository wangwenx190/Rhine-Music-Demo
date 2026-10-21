// Defines source-space case dimensions and archive placement shared by scene consumers.
// Values use the imported mesh coordinate system; no device-pixel or backend scaling applies.

#pragma once

namespace yalahoo::sceneGeometry {
inline constexpr double ColumnSpacing = 5.2;
inline constexpr double RowSpacing = .62;
inline constexpr double FloorDepth = 4.6;
inline constexpr double ArchiveDepth = 2.17;
inline constexpr double CaseBottom = .175;
inline constexpr double CaseHeight = 3.35;
inline constexpr double CaseTop = 3.525;
inline constexpr double CaseHalfWidth = 2.225;
inline constexpr double CaseHalfDepth = .07;
inline constexpr double HingeHeight = 1.85;
inline constexpr double PreviewLift = .9;
inline constexpr double InspectionLift = 3.47;
inline constexpr double SettledSceneSeconds = 26.56;
inline constexpr auto FirstAlbumRow = 12;
inline constexpr auto DetailFollowRate = 9.0;
inline constexpr auto BrowseFollowRate = 3.7;
inline constexpr auto InspectionLiftPadding = .12;
inline constexpr auto ParallelRayTolerance = 1e-10;
}
