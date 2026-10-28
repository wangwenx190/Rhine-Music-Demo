// Evaluates the source opening timeline as a value snapshot. Named time windows keep visibility,
// blur and geometry synchronized without independent timers.

#pragma once
#include "SceneFrame.h"
#include <QVariantMap>

namespace yalahoo {
namespace openingFrameDefaults {
// Initial two-dimensional opening state before timeline evaluation.
inline constexpr auto OrbitScale = .9;
inline constexpr auto TitleEnterOffsetPixels = 18;
inline constexpr auto TitleClipPercent = 100;
}

struct OpeningFrame final
{
    double background = 1, mark = 0, contour = 0, symbols = 0;
    double orbit = 0, orbitDraw = 0, orbitTurn = 0, orbitScale = openingFrameDefaults::OrbitScale;
    double title = 0, titleY = openingFrameDefaults::TitleEnterOffsetPixels,
           titleClip = openingFrameDefaults::TitleClipPercent, subtitle = 0;
    enum class Phase {
        Logo,
        Orbit,
        Slogan,
        Array,
    };
    Phase phase = Phase::Logo;
    QVariantMap values() const;
};
OpeningFrame openingFrame(double time);
double openingSceneTime(double time);
CameraPose openingCamera(double sceneTime, double width, double height, SceneVector selected);
double openingReveal(double elapsed, double delay = 0);
}
