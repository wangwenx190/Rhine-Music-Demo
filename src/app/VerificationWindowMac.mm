#include "VerificationWindow.h"
#include <QEvent>
#include <QPlatformSurfaceEvent>
#include <QTimer>
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

namespace {
constexpr NSInteger VerificationLevel = NSNormalWindowLevel - 1;
pid_t foregroundPid = 0;
bool activationObserved = false;
bool focusObserved = false;
id activationObserver = nil;
}

// Observe public Cocoa properties synchronously: Qt can reset the level or replace
// the NSWindow while showing/recreating it, after the QWindow Show event.
@interface YalahooVerificationObserver : NSObject {
    NSView *view;
    NSWindow *nativeWindow;
}
- (instancetype)initWithView:(NSView *)nativeView;
- (void)bindWindow;
- (void)focusChanged:(NSNotification *)notification;
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
        [NSNotificationCenter.defaultCenter removeObserver:self];
        [nativeWindow removeObserver:self forKeyPath:@"level"];
        [nativeWindow release];
        nativeWindow = [view.window retain];
        [nativeWindow addObserver:self forKeyPath:@"level" options:0 context:nullptr];
        for (NSString *name in
             @[ NSWindowDidBecomeKeyNotification, NSWindowDidBecomeMainNotification ])
            [NSNotificationCenter.defaultCenter addObserver:self
                                                   selector:@selector(focusChanged:)
                                                       name:name
                                                     object:nativeWindow];
    }
    const NSInteger level = yalahoo::verification::foregroundVerification() ? NSNormalWindowLevel
                                                                            : VerificationLevel;
    if (nativeWindow.level != level)
        nativeWindow.level = level;
    nativeWindow.collectionBehavior |= NSWindowCollectionBehaviorIgnoresCycle;
    nativeWindow.ignoresMouseEvents = YES;
}
- (void)focusChanged:(NSNotification *)notification
{
    Q_UNUSED(notification);
    focusObserved = true;
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
    [NSNotificationCenter.defaultCenter removeObserver:self];
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
            if (event->type() == QEvent::Show && yalahoo::verification::foregroundVerification()) {
                QTimer::singleShot(0, this, [this] {
                    if (!m_window.isVisible())
                        return;
                    [NSApp unhide:nil];
                    if (@available(macOS 14.0, *))
                        [NSApp activate];
                    else
                        [NSApp activateIgnoringOtherApps:YES];
                    m_window.raise();
                    m_window.requestActivate();
                });
            }
        }
        return false;
    }

private:
    QWindow &m_window;
    YalahooVerificationObserver *m_observer = nil;
};
}

void yalahoo::verification::prepareNativeApplication()
{
    if (!foregroundPid)
        foregroundPid = NSWorkspace.sharedWorkspace.frontmostApplication.processIdentifier;
    NSApplication *application = NSApplication.sharedApplication;
    if (!activationObserver) {
        activationObserver = [NSNotificationCenter.defaultCenter
                addObserverForName:NSApplicationDidBecomeActiveNotification
                            object:application
                             queue:nil
                        usingBlock:^(NSNotification *) {
                            activationObserved = true;
                        }];
        [application setActivationPolicy:foregroundVerification()
                             ? NSApplicationActivationPolicyRegular
                             : NSApplicationActivationPolicyProhibited];
    }
}

void yalahoo::verification::prepareNativeWindow(QWindow &window)
{
    prepareNativeApplication();
    if (foregroundVerification())
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
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
    const pid_t currentForegroundPid =
            NSWorkspace.sharedWorkspace.frontmostApplication.processIdentifier;
    const bool foregroundUnchanged = foregroundPid && currentForegroundPid == foregroundPid;
    const bool safe = foregroundVerification()
            ? NSApp.activationPolicy == NSApplicationActivationPolicyRegular
                    && (!window.handle() || (native && native.level == NSNormalWindowLevel))
            : (!window.handle()
               || (native.level == VerificationLevel && !native.canBecomeKeyWindow
                   && native.ignoresMouseEvents && !native.keyWindow && !native.mainWindow))
                    && prohibited && launchActivationDisabled && !NSApp.active
                    && foregroundUnchanged && !activationObserved && !focusObserved;
    bool onScreen = false;
    if (native) {
        const auto info = CGWindowListCopyWindowInfo(kCGWindowListOptionIncludingWindow,
                                                     CGWindowID(native.windowNumber));
        if (info) {
            for (NSDictionary *entry in reinterpret_cast<NSArray *>(info))
                onScreen = onScreen ||
                        [entry[reinterpret_cast<NSString *>(kCGWindowIsOnscreen)] boolValue];
            CFRelease(info);
        }
    }
    return { { "passed", safe },
             { "nativeWindowAvailable", native != nil },
             { "visible", native && bool(native.visible) },
             { "onScreen", onScreen },
             { "miniaturized", native && bool(native.miniaturized) },
             { "occlusionVisible",
               native && bool(native.occlusionState & NSWindowOcclusionStateVisible) },
             { "level", native ? int(native.level) : int(VerificationLevel) },
             { "belowNormal", !native || native.level < NSNormalWindowLevel },
             { "canBecomeKey", native && native.canBecomeKeyWindow },
             { "canBecomeMain", native && native.canBecomeMainWindow },
             { "applicationActive", bool(NSApp.active) },
             { "applicationHidden", bool(NSApp.hidden) },
             { "activationProhibited", prohibited },
             { "activationObserved", activationObserved },
             { "focusObserved", focusObserved },
             { "launchActivationDisabled", launchActivationDisabled },
             { "ignoresMouseEvents", !native || bool(native.ignoresMouseEvents) },
             { "foregroundUnchanged", foregroundUnchanged },
             { "initialForegroundPid", int(foregroundPid) },
             { "currentForegroundPid", int(currentForegroundPid) },
             { "verificationPid", int(NSProcessInfo.processInfo.processIdentifier) } };
}
