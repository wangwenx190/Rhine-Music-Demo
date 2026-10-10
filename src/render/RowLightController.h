#pragma once
#include "SceneFrame.h"
#include "ScalarSpring.h"
#include <optional>
namespace yalahoo {
enum class RowLightingMode { Baseline, Area, Guided, Hybrid };
struct RowLightingSettings {
    RowLightingMode mode = RowLightingMode::Guided;
    double strength = 1.3, width = 1.35, offset = .3, fill = 1, transitionDuration = 1.1;
};
struct RowLightingInput {
    SceneVector origin;
    double row = 0, lane = 0;
    bool independentColumns = false, reduced = false, cinematic = false, visible = true;
    std::optional<double> introTime;
};
// Absolute logical coordinates; the rendered shelf transform is added only once.
class RowLightController {
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
