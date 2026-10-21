// Evaluates bounded per-row lighting pulses from selection events. Elapsed-time envelopes avoid
// retaining an unbounded history of UI interactions.

#pragma once
#include "SceneFrame.h"
#include "ScalarSpring.h"
#include <optional>
namespace yalahoo {
namespace rowLightDefaults {
// Source guided-row lighting defaults, independent of viewport/DPR.
inline constexpr auto Strength = 1.3;
inline constexpr auto WidthRows = 1.35;
inline constexpr auto OffsetRows = .3;
inline constexpr auto TransitionSeconds = 1.1;
}

enum class RowLightingMode {
    Baseline,
    Area,
    Guided,
    Hybrid
};
struct RowLightingSettings final
{
    RowLightingMode mode = RowLightingMode::Guided;
    double strength = rowLightDefaults::Strength, width = rowLightDefaults::WidthRows,
           offset = rowLightDefaults::OffsetRows, fill = 1,
           transitionDuration = rowLightDefaults::TransitionSeconds;
    bool operator==(const RowLightingSettings &) const = default;
};
struct RowLightingInput final
{
    SceneVector origin;
    double row = 0, lane = 0;
    bool independentColumns = false, reduced = false, cinematic = false, visible = true;
    std::optional<double> introTime{ };
};
// Absolute logical coordinates; the rendered shelf transform is added only once.
class RowLightController final
{
public:
    RowLightingSettings settings;
    ScalarSpring row, lane;
    SceneVector column;
    bool opening = false;
    void reset();
    void update(const RowLightingInput &input, double motionDt);
    static double follow(ScalarSpring &state, double target, double dt, double duration);
    static double introOffset(double time);
    double sample(SceneVector origin) const;

private:
    bool initialized = false, independent = false;
    std::optional<double> basis;
    double basisLane = 0;
};
}
