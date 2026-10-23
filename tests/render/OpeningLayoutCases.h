// Selects real native DPR or controlled RenderControl DPRs before any opening captures.
// Each case must finish all film checkpoints before the layout matrix can pass.
#pragma once
#include "Require.h"
#include <QList>
#include <QSize>
#include <cmath>

namespace yalahoo::review {
struct OpeningLayoutCase final
{
    bool night;
    double dpr;
    QSize size;
};
inline QList<OpeningLayoutCase> openingLayoutCases(bool offscreen, double nativeDpr,
                                                   bool representative)
{
    require(std::isfinite(nativeDpr) && nativeDpr > 0, "Invalid native opening DPR");
    const QList<double> dprs = offscreen ? QList<double>{ 1, 1.5, 2 } : QList<double>{ nativeDpr };
    const QList<QSize> sizes{ { 1920, 1080 }, { 1280, 720 },  { 995, 858 },
                              { 960, 900 },   { 930, 900 },   { 390, 844 },
                              { 320, 720 },   { 2560, 1080 }, { 3440, 1440 } };
    QList<OpeningLayoutCase> result;
    for (bool night : { false, true }) {
        for (double dpr : dprs) {
            for (const auto &size : sizes) {
                if (representative && (dpr != dprs.first() || night) && size != QSize(1280, 720)
                    && size != QSize(390, 844))
                    continue;
                result.append({ night, dpr, size });
            }
        }
    }
    return result;
}
}
