#pragma once
#include <QString>
using Qt::StringLiterals::operator""_s;
#include "SceneFrame.h"
#include <QVariantMap>

namespace yalahoo {
struct OpeningFrame
{
    double background = 1, mark = 0, contour = 0, symbols = 0;
    double orbit = 0, orbitDraw = 0, orbitTurn = 0, orbitScale = .9;
    double title = 0, titleY = 18, titleClip = 100, subtitle = 0;
    QString phase = u"logo"_s;
    QVariantMap values() const;
};
OpeningFrame openingFrame(double time);
double openingSceneTime(double time);
CameraPose openingCamera(double sceneTime, double width, double height, SceneVector selected);
double openingReveal(double elapsed, double delay = 0);
}
