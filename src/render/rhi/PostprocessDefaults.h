// Shares source postprocessing defaults without exposing RHI resource APIs.

#pragma once
#include "SceneFrame.h"

namespace yalahoo::rhi {
namespace postDefaults {
// Source scene clip/focus distances and postprocessing defaults.
inline constexpr auto FocusDistance = 25;
inline constexpr auto Aperture = .0003f;
inline constexpr auto AoRadius = .18f;
}

}
