#pragma once
#include <QObject>
#include <functional>
#include <memory>

class QWindow;

namespace yalahoo {
struct DisplayState
{
    bool enabled = false;
    bool hdr10 = false;
    float whiteScale = 1;
    float headroom = 1;
    bool allowLinearFallback = false;
    friend bool operator==(const DisplayState &, const DisplayState &) = default;
};

// Platform callbacks and query() run on the GUI thread. Missing information is SDR.
class DisplayMonitor : public QObject
{
public:
    using QObject::QObject;
    virtual DisplayState query(QWindow &window) = 0;
    std::function<void()> changed;
};
std::unique_ptr<DisplayMonitor> makeDisplayMonitor();
}
