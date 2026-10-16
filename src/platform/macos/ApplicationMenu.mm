#include "ApplicationMenu.h"
#include <QGuiApplication>
#include <QMetaObject>
#include <QPointer>
#include <QQuickWindow>
#include <QVariant>
#include <utility>
#import <AppKit/AppKit.h>

@interface YalahooMenuTarget : NSObject <NSMenuItemValidation> {
@public
    QPointer<QQuickWindow> window;
    std::function<void()> settings;
    std::function<bool()> settingsEnabled;
}
- (void)openSettings:(id)sender;
- (void)closeWindow:(id)sender;
- (void)minimizeWindow:(id)sender;
- (void)zoomWindow:(id)sender;
- (void)quit:(id)sender;
- (void)edit:(NSMenuItem *)sender;
@end

namespace {
const char *editMethod(NSInteger tag)
{
    switch (tag) {
    case 1:
        return "undo";
    case 2:
        return "redo";
    case 3:
        return "cut";
    case 4:
        return "copy";
    case 5:
        return "paste";
    case 6:
        return "selectAll";
    default:
        return nullptr;
    }
}
QObject *editor(QQuickWindow *window)
{
    auto *focus = window ? window->focusObject() : nullptr;
    return focus && focus->metaObject()->indexOfMethod("selectAll()") >= 0 ? focus : nullptr;
}
NSWindow *nativeWindow(QQuickWindow *window)
{
    if (!window || !window->handle())
        return nil;
    auto *view = reinterpret_cast<NSView *>(window->winId());
    return view.window;
}
bool nativeDialog(QQuickWindow *window)
{
    auto *owner = nativeWindow(window);
    return NSApp.modalWindow || owner.attachedSheet
            || (NSApp.keyWindow && NSApp.keyWindow != owner);
}
SEL nativeEditMethod(NSInteger tag)
{
    const auto *method = editMethod(tag);
    return method ? NSSelectorFromString(QString::fromLatin1(method).append(':').toNSString())
                  : nil;
}
bool nativeEditEnabled(NSMenuItem *item)
{
    const SEL selector = nativeEditMethod(item.tag);
    id target = selector ? [NSApp targetForAction:selector to:nil from:item] : nil;
    if (!target)
        return false;
    // Cocoa text fields in native file dialogs keep their own editing semantics.
    auto *nativeItem = [[[NSMenuItem alloc] initWithTitle:item.title
                                                   action:selector
                                            keyEquivalent:item.keyEquivalent] autorelease];
    if ([target respondsToSelector:@selector(validateMenuItem:)])
        return [target validateMenuItem:nativeItem];
    if ([target respondsToSelector:@selector(validateUserInterfaceItem:)])
        return [target validateUserInterfaceItem:nativeItem];
    return true;
}
NSMenu *addMenu(NSMenu *bar, NSString *title)
{
    auto *item = [[[NSMenuItem alloc] initWithTitle:title action:nil
                                      keyEquivalent:@""] autorelease];
    auto *menu = [[[NSMenu alloc] initWithTitle:title] autorelease];
    item.submenu = menu;
    [bar addItem:item];
    return menu;
}
NSMenuItem *addItem(NSMenu *menu, NSString *title, NSString *key, SEL action, id target)
{
    auto *item = [[[NSMenuItem alloc] initWithTitle:title action:action
                                      keyEquivalent:key] autorelease];
    item.target = target;
    [menu addItem:item];
    return item;
}
}

@implementation YalahooMenuTarget
- (BOOL)validateMenuItem:(NSMenuItem *)item
{
    if (item.action == @selector(quit:))
        return YES;
    if (!window)
        return NO;
    if (nativeDialog(window))
        return item.action == @selector(edit:) && nativeEditEnabled(item);
    if (item.action == @selector(openSettings:))
        return settingsEnabled && settingsEnabled();
    if (item.action != @selector(edit:))
        return window->isVisible();
    auto *focus = editor(window);
    const auto *method = editMethod(item.tag);
    if (!focus || !method || focus->metaObject()->indexOfMethod(QByteArray(method) + "()") < 0)
        return NO;
    if (item.tag == 4)
        return !focus->property("selectedText").toString().isEmpty();
    if (item.tag == 6)
        return !focus->property("text").toString().isEmpty();
    if (focus->property("readOnly").toBool())
        return NO;
    if (item.tag == 1)
        return focus->property("canUndo").toBool();
    if (item.tag == 2)
        return focus->property("canRedo").toBool();
    if (item.tag == 3)
        return !focus->property("selectedText").toString().isEmpty();
    return focus->property("canPaste").toBool();
}
- (void)openSettings:(id)sender
{
    Q_UNUSED(sender);
    if (window && !nativeDialog(window) && settings && settingsEnabled && settingsEnabled())
        settings();
}
- (void)closeWindow:(id)sender
{
    Q_UNUSED(sender);
    if (window)
        window->close();
}
- (void)minimizeWindow:(id)sender
{
    Q_UNUSED(sender);
    if (window)
        window->showMinimized();
}
- (void)zoomWindow:(id)sender
{
    Q_UNUSED(sender);
    if (window) {
        if (window->visibility() == QWindow::Maximized)
            window->showNormal();
        else
            window->showMaximized();
    }
}
- (void)quit:(id)sender
{
    Q_UNUSED(sender);
    QCoreApplication::quit();
}
- (void)edit:(NSMenuItem *)sender
{
    if (![self validateMenuItem:sender])
        return;
    if (nativeDialog(window)) {
        [NSApp sendAction:nativeEditMethod(sender.tag) to:nil from:sender];
        return;
    }
    if (auto *focus = editor(window))
        QMetaObject::invokeMethod(focus, editMethod(sender.tag), Qt::DirectConnection);
}
@end

namespace yalahoo {
struct ApplicationMenu::State
{
    NSMenu *previous = nil;
    NSMenu *bar = nil;
    YalahooMenuTarget *target = nil;
    ~State()
    {
        if (NSApp.mainMenu == bar)
            NSApp.mainMenu = previous;
        [bar release];
        [target release];
        [previous release];
    }
};
ApplicationMenu::ApplicationMenu(QQuickWindow &window, std::function<void()> settings,
                                 std::function<bool()> settingsEnabled)
    : m_state(std::make_unique<State>())
{
    m_state->previous = [NSApp.mainMenu retain];
    m_state->target = [[YalahooMenuTarget alloc] init];
    m_state->target->window = &window;
    m_state->target->settings = std::move(settings);
    m_state->target->settingsEnabled = std::move(settingsEnabled);
    m_state->bar = [[NSMenu alloc] initWithTitle:@"yalahOO!!!"];
    auto *application = addMenu(m_state->bar, @"yalahOO!!!");
    addItem(application, @"设置…", @",", @selector(openSettings:), m_state->target);
    [application addItem:[NSMenuItem separatorItem]];
    addItem(application, @"隐藏 yalahOO!!!", @"h", @selector(hide:), NSApp);
    auto *hideOthers =
            addItem(application, @"隐藏其他", @"h", @selector(hideOtherApplications:), NSApp);
    hideOthers.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagOption;
    addItem(application, @"全部显示", @"", @selector(unhideAllApplications:), NSApp);
    [application addItem:[NSMenuItem separatorItem]];
    addItem(application, @"退出 yalahOO!!!", @"q", @selector(quit:), m_state->target);
    auto *file = addMenu(m_state->bar, @"文件");
    addItem(file, @"关闭", @"w", @selector(closeWindow:), m_state->target);
    auto *edit = addMenu(m_state->bar, @"编辑");
    const NSArray<NSString *> *titles = @[ @"撤销", @"重做", @"剪切", @"复制", @"粘贴", @"全选" ];
    const NSArray<NSString *> *keys = @[ @"z", @"z", @"x", @"c", @"v", @"a" ];
    for (NSInteger index = 0; index < 6; ++index) {
        auto *item = addItem(edit, titles[index], keys[index], @selector(edit:), m_state->target);
        item.tag = index + 1;
        if (index == 1)
            item.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;
        if (index == 1 || index == 4)
            [edit addItem:[NSMenuItem separatorItem]];
    }
    auto *windowMenu = addMenu(m_state->bar, @"窗口");
    addItem(windowMenu, @"最小化", @"m", @selector(minimizeWindow:), m_state->target);
    addItem(windowMenu, @"缩放", @"", @selector(zoomWindow:), m_state->target);
    NSApp.mainMenu = m_state->bar;
}
ApplicationMenu::~ApplicationMenu() = default;
}
