#include "VerificationWindow.h"
#include <QEvent>
#include <QPlatformSurfaceEvent>
#import <AppKit/AppKit.h>

namespace {
constexpr NSInteger VerificationLevel = NSNormalWindowLevel - 1;
pid_t foregroundPid = 0;
}

// Observe public Cocoa properties synchronously: Qt can reset the level or replace
// the NSWindow while showing/recreating it, after the QWindow Show event.
@interface YalahooVerificationObserver : NSObject {
    NSView *view;
    NSWindow *nativeWindow;
}
- (instancetype)initWithView:(NSView *)nativeView;
- (void)bindWindow;
@end

@implementation YalahooVerificationObserver
- (instancetype)initWithView:(NSView *)nativeView
{
    self = [super init];
    if (self) {
        view = [nativeView retain];
        [view addObserver:self forKeyPath:@"window" options:0 context:nullptr];
        [self bindWindow];
    }
    return self;
}
- (void)bindWindow
{
    if (nativeWindow != view.window) {
        [nativeWindow removeObserver:self forKeyPath:@"level"];
        [nativeWindow release];
        nativeWindow = [view.window retain];
        [nativeWindow addObserver:self forKeyPath:@"level" options:0 context:nullptr];
    }
    if (nativeWindow.level != VerificationLevel)
        nativeWindow.level = VerificationLevel;
    nativeWindow.collectionBehavior |= NSWindowCollectionBehaviorIgnoresCycle;
    nativeWindow.ignoresMouseEvents = YES;
}
- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context
{
    Q_UNUSED(object);
    Q_UNUSED(change);
    Q_UNUSED(context);
    if ([keyPath isEqualToString:@"window"] || [keyPath isEqualToString:@"level"])
        [self bindWindow];
}
- (void)dealloc
{
    [view removeObserver:self forKeyPath:@"window"];
    [nativeWindow removeObserver:self forKeyPath:@"level"];
    [nativeWindow release];
    [view release];
    [super dealloc];
}
@end

namespace {
class NativeWindowGuard : public QObject
{
public:
    explicit NativeWindowGuard(QWindow &window) : QObject(&window), m_window(window)
    {
        setObjectName("yalahooVerificationNativeGuard");
        window.installEventFilter(this);
    }
    ~NativeWindowGuard() override
    {
        [m_observer release];
    }
    void bind()
    {
        if (!m_window.handle())
            return;
        if (!m_observer)
            m_observer = [[YalahooVerificationObserver alloc]
                    initWithView:reinterpret_cast<NSView *>(m_window.winId())];
        [m_observer bindWindow];
    }

protected:
    bool eventFilter(QObject *, QEvent *event) override
    {
        if (event->type() == QEvent::PlatformSurface) {
            const auto *surface = static_cast<QPlatformSurfaceEvent *>(event);
            if (surface->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
                [m_observer release];
                m_observer = nil;
            } else {
                bind();
            }
        } else if (event->type() == QEvent::Show || event->type() == QEvent::WinIdChange) {
            bind();
        }
        return false;
    }

private:
    QWindow &m_window;
    YalahooVerificationObserver *m_observer = nil;
};
}

void yalahoo::verification::prepareNativeWindow(QWindow &window)
{
    if (!foregroundPid)
        foregroundPid = NSWorkspace.sharedWorkspace.frontmostApplication.processIdentifier;
    // Verification processes must never activate themselves or gain a Dock entry.
    [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];
    auto *guard = static_cast<NativeWindowGuard *>(window.findChild<QObject *>(
            "yalahooVerificationNativeGuard", Qt::FindDirectChildrenOnly));
    if (!guard)
        guard = new NativeWindowGuard(window);
    window.create();
    guard->bind();
}

QJsonObject yalahoo::verification::nativeWindowPolicy(const QWindow &window)
{
    NSWindow *native = nil;
    if (window.handle())
        native = reinterpret_cast<NSView *>(window.winId()).window;
    const bool prohibited = NSApp.activationPolicy == NSApplicationActivationPolicyProhibited;
    const bool launchActivationDisabled =
            !qgetenv("QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM").isEmpty();
    const bool foregroundUnchanged = foregroundPid
            && NSWorkspace.sharedWorkspace.frontmostApplication.processIdentifier == foregroundPid;
    const bool safe = (!window.handle()
                       || (native.level == VerificationLevel && !native.canBecomeKeyWindow
                           && native.ignoresMouseEvents && !native.keyWindow && !native.mainWindow))
            && prohibited && launchActivationDisabled && !NSApp.active;
    return { { "passed", safe },
             { "nativeWindowAvailable", native != nil },
             { "level", native ? int(native.level) : int(VerificationLevel) },
             { "belowNormal", !native || native.level < NSNormalWindowLevel },
             { "canBecomeKey", native && native.canBecomeKeyWindow },
             { "canBecomeMain", native && native.canBecomeMainWindow },
             { "applicationActive", bool(NSApp.active) },
             { "activationProhibited", prohibited },
             { "launchActivationDisabled", launchActivationDisabled },
             { "ignoresMouseEvents", !native || bool(native.ignoresMouseEvents) },
             { "foregroundUnchanged", foregroundUnchanged } };
}
