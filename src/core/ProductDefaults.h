// Owns user preference defaults and shared speed bounds for persistence and playback.
// Product initialization and independent controllers consume the same policy values.

#pragma once

namespace yalahoo::productDefaults {
inline constexpr auto MinimumMotionSpeed = .25;
inline constexpr auto MaximumMotionSpeed = 3.0;
inline constexpr auto DefaultSongVolume = .65;
inline constexpr auto DefaultBackgroundVolume = .18;
inline constexpr auto DefaultFeedbackVolume = .22;
}
