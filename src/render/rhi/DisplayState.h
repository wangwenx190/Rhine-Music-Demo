// Defines the platform-neutral display capability snapshot and monitor boundary. Missing native
// information yields a fresh SDR state rather than stale HDR values.

#pragma once
#include <QObject>
#include <functional>
#include <memory>

class QWindow;

namespace yalahoo {
struct DisplayState final
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
    Q_DISABLE_COPY_MOVE(DisplayMonitor)
public:
    DisplayMonitor() = default;
    using QObject::QObject;
    virtual DisplayState query(QWindow &window) = 0;
    // A fresh SDR snapshot can be provisional during asynchronous native discovery.
    virtual bool pending() const
    {
        return false;
    }
    std::function<void()> changed;
};
std::unique_ptr<DisplayMonitor> makeDisplayMonitor();
}
