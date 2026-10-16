#include "OpeningFrame.h"
#include "MotionFunctions.h"
#include <algorithm>
#include <numbers>

namespace yalahoo {
namespace {
double unit(double t, double from, double to)
{
    return std::clamp((t - from) / (to - from), 0.0, 1.0);
}
double openingSmooth(double t)
{
    return t * t * (3 - 2 * t);
}
double settle(double t)
{
    return 1 - std::pow(1 - t, 3);
}
}
OpeningFrame openingFrame(double time)
{
    const double t = std::isfinite(time) ? std::max(0.0, time) : 0;
    const double in = settle(unit(t, 2.72, 3.58)), out = openingSmooth(unit(t, 5.7, 6.32));
    OpeningFrame f;
    f.phase = t < 1.8 ? "logo" : t < 2.9 ? "orbit" : t < 6.4 ? "slogan" : "array";
    f.background = 1 - openingSmooth(unit(t, 6.32, 6.82));
    f.mark = openingSmooth(unit(t, .2, .48)) * (1 - openingSmooth(unit(t, 1.8, 2.26)));
    f.contour = settle(unit(t, .24, 1.64));
    f.symbols = settle(unit(t, .84, 1.5));
    f.orbit = openingSmooth(unit(t, 1.64, 2.05)) * (1 - openingSmooth(unit(t, 2.78, 3.28)));
    f.orbitDraw = settle(unit(t, 1.64, 2.82));
    f.orbitTurn = 190 * openingSmooth(unit(t, 1.64, 3.3));
    f.orbitScale = .9 + .1 * settle(unit(t, 1.64, 2.8));
    f.title = in * (1 - out);
    f.titleY = 18 * (1 - in) - 8 * out;
    f.titleClip = 100 * (1 - in);
    f.subtitle = openingSmooth(unit(t, 3.18, 3.8)) * (1 - openingSmooth(unit(t, 5.56, 6.08)));
    return f;
}
QVariantMap OpeningFrame::values() const
{
    return { { "background", background },
             { "mark", mark },
             { "contour", contour },
             { "symbols", symbols },
             { "orbit", orbit },
             { "orbitDraw", orbitDraw },
             { "orbitTurn", orbitTurn },
             { "orbitScale", orbitScale },
             { "title", title },
             { "titleY", titleY },
             { "titleClip", titleClip },
             { "subtitle", subtitle },
             { "phase", phase } };
}
double openingSceneTime(double time)
{
    return 21.92 + std::max(0.0, time - 6.4);
}
CameraPose openingCamera(double sceneTime, double width, double height, SceneVector selected)
{
    const double shot = std::min(sceneTime, 27.12), aspect = width / height;
    const double orbit = smooth((shot - 22.6) / 1.6), settle = smooth((shot - 24.25) / 2.25);
    const double intro = smooth((shot - 25.3) / 1.42);
    const double radians = std::numbers::pi / 180;
    const double yaw = (89 - 22 * orbit - 8 * settle) * radians;
    const double elevation =
            (3 + 40 * smooth((shot - 21.96) / .22) - 8 * orbit - 16 * settle + 6 * intro) * radians;
    const SceneVector direction{ -std::sin(yaw) * std::cos(elevation), std::sin(elevation),
                                 std::cos(yaw) * std::cos(elevation) };
    const auto right = cross({ 0, 1, 0 }, direction).normalized(),
               up = cross(direction, right).normalized();
    const double sourceSpan = (10.8 + (10.3 - 10.8) * orbit) * (1 - settle) + 7.33 * settle;
    const double span = aspect < 1.05 ? std::max(sourceSpan, 8.4 / aspect)
                                      : std::max(sourceSpan, sourceSpan * (16.0 / 9) / aspect);
    const double distance = (28 + 7 * orbit) * (1 - settle) + 140 * settle;
    SceneVector array{ -1.091, (-2.55 + .4 * orbit) * (1 - settle) - .045 * settle,
                       2.48 + (.481 - 2.48) * settle };
    auto aim = array
            + right * (-2.05 * (1 - smooth((shot - 25.4) / .95)) * smooth((shot - 24.2) / .8));
    if (shot >= 25.05 && shot <= 27.3) {
        const double pan = smooth((shot - 25.4) / 1.05), scale = 1080 / sourceSpan;
        auto anchor = selected + SceneVector{ -4.45 / 2, 1.85 + 3.35 / 2, 0 };
        anchor = anchor + right * (-(840 + (518 - 840) * pan - 960) / scale)
                + up * (-(540 - (340 + (288 - 340) * pan)) / scale);
        const double blend = smooth((shot - 25.05) / .35);
        aim = aim * (1 - blend) + anchor * blend;
    }
    auto preview = array;
    if (aspect < 1.05)
        preview = SceneVector{ 0, -4.6 + settlingWave(0, 26.56) + .4 + 1.85, -2.17 }
                + up * ((.36 - .5) * span);
    else {
        const double wide = smooth((aspect - 16.0 / 9) / (21.0 / 9 - 16.0 / 9));
        preview =
                preview + right * (.035 * wide * span * aspect) + up * ((.08 + .02 * wide) * span);
    }
    aim = aim * (1 - intro) + preview * intro;
    return { aim + direction * distance, aim, span };
}
double openingReveal(double elapsed, double delay)
{
    const double x = unit(elapsed, delay, .72);
    // Invert CSS cubic-bezier(.22,1,.36,1), retaining the authored stagger endpoint.
    double lo = 0, hi = 1;
    for (int i = 0; i < 40; ++i) {
        const double t = (lo + hi) * .5, u = 1 - t;
        const double bx = 3 * u * u * t * .22 + 3 * u * t * t * .36 + t * t * t;
        if (bx < x)
            lo = t;
        else
            hi = t;
    }
    return x == 0 ? 0 : x == 1 ? 1 : 1 - std::pow(1 - (lo + hi) * .5, 3);
}
}
