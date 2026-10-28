// Shares authored opening times between the visual clock, source camera and handoff state.
// Values are seconds on the named opening or source-scene timeline, never wall-clock time.

#pragma once
namespace yalahoo::openingTimeline {
inline constexpr auto DurationSeconds = 11.6;
inline constexpr auto TerminalSceneSeconds = 27.12;
inline constexpr auto SceneOriginSeconds = 21.92;
inline constexpr auto PreviewStartSeconds = 25.3;
inline constexpr auto PreviewDurationSeconds = 1.42;
inline constexpr auto CompletionEpsilonSeconds = 1e-9;
}
