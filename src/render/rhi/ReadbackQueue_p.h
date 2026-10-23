// Internal capture-completion state. Tests can hold individual backend completions to
// verify bounded continuation and retirement without claiming a physical GPU event sequence.
#pragma once
#include "ReadbackQueue.h"

namespace yalahoo::rhi {
struct ReadbackQueue::Pending final
{
    Q_DISABLE_COPY_MOVE(Pending)
    Pending() = default;
    std::array<QRhiReadbackResult, CaptureFieldCount> results;
    Capture capture;
    std::function<void(Capture)> deliver;
    std::function<void(QString)> failed;
    int remaining = 0;
    std::array<int, CaptureFieldCount> readbackOwner;
    int drainFrames = 0;
    bool yUp = false;
    int opticalProbe = 0;
    bool captureShadowDepth = false;
    std::array<QSize, CaptureFieldCount> logicalReadbackSizes;
};
}
