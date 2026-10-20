#include "DisplayState.h"
#include <QScreen>
#include <QPointer>
#include <QWindow>
#include <QtGui/qscreen_platform.h>
#import <AppKit/AppKit.h>
#include <algorithm>

namespace yalahoo {
class MacDisplayMonitor final : public DisplayMonitor
{
    Q_DISABLE_COPY_MOVE(MacDisplayMonitor)
public:
    MacDisplayMonitor()
    {
        const QPointer<DisplayMonitor> guard(this);
        m_observer = [[NSNotificationCenter defaultCenter]
                addObserverForName:NSApplicationDidChangeScreenParametersNotification
                            object:nil
                             queue:[NSOperationQueue mainQueue]
                        usingBlock:^(NSNotification *) {
                            if (guard && guard->changed)
                                guard->changed();
                        }];
    }
    ~MacDisplayMonitor() override
    {
        [[NSNotificationCenter defaultCenter] removeObserver:m_observer];
    }
    DisplayState query(QWindow &window) override
    {
        auto *screen = window.screen();
        auto *native = screen ? screen->nativeInterface<QNativeInterface::QCocoaScreen>() : nullptr;
        NSScreen *output = native ? native->nativeScreen() : nil;
        if (!output || output.maximumPotentialExtendedDynamicRangeColorComponentValue <= 1)
            return { };
        // macOS manages EDR automatically; 1.0 is the current SDR white. Current
        // headroom can reach 1 at full brightness without losing EDR capability.
        return { true, false, 1,
                 float(std::max(1.0, output.maximumExtendedDynamicRangeColorComponentValue)) };
    }

private:
    id m_observer = nil;
};
std::unique_ptr<DisplayMonitor> makeDisplayMonitor()
{
    return std::make_unique<MacDisplayMonitor>();
}
}
