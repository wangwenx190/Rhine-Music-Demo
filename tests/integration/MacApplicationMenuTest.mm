#include "ApplicationMenu.h"
#include "VerificationWindow.h"
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <memory>
#import <AppKit/AppKit.h>

using namespace Qt::StringLiterals;

namespace {
NSMenuItem *action(NSMenu *menu, NSString *selector, NSInteger tag = 0)
{
    for (NSMenuItem *item in menu.itemArray) {
        if (item.action == NSSelectorFromString(selector) && (!tag || item.tag == tag))
            return item;
        if (item.submenu) {
            if (auto *found = action(item.submenu, selector, tag))
                return found;
        }
    }
    return nil;
}
void invoke(NSMenuItem *item)
{
    [NSApp sendAction:item.action to:item.target from:item];
}
}
class MacApplicationMenuTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MacApplicationMenuTest)
public:
    MacApplicationMenuTest() = default;

private Q_SLOTS:
    void commandsAndOwnership()
    {
        QQuickWindow window;
        yalahoo::verification::prepareWindow(window);
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(
                "import QtQuick\nTextInput { text: '\345\214\277\345\220\215 Music'; width: 300; height: 40 }"_ba,
                QUrl());
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *input = qobject_cast<QQuickItem *>(object.get());
        QVERIFY(input);
        input->setParentItem(window.contentItem());
        input->forceActiveFocus();
        QCOMPARE(window.focusObject(), object.get());
        bool allowed = false;
        int opened = 0;
        auto *previous = NSApp.mainMenu;
        auto menu = std::make_unique<yalahoo::ApplicationMenu>(
                window,
                [&] {
                    ++opened;
                },
                [&] {
                    return allowed;
                });
        auto *bar = NSApp.mainMenu;
        QVERIFY(bar != previous);
        auto *settings = action(bar, @"openSettings:");
        QVERIFY(settings);
        QCOMPARE(QString::fromNSString(settings.keyEquivalent), u","_s);
        QVERIFY(![settings.target validateMenuItem:settings]);
        invoke(settings);
        QCOMPARE(opened, 0);
        allowed = true;
        QVERIFY([settings.target validateMenuItem:settings]);
        invoke(settings);
        QCOMPARE(opened, 1);
        auto *commandComma = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                              location:NSZeroPoint
                                         modifierFlags:NSEventModifierFlagCommand
                                             timestamp:0
                                          windowNumber:0
                                               context:nil
                                            characters:@","
                           charactersIgnoringModifiers:@","
                                             isARepeat:NO
                                               keyCode:43];
        QVERIFY([bar performKeyEquivalent:commandComma]);
        QCOMPARE(opened, 2);
        QTest::qWait(1);
        QVERIFY(NSApp.mainMenu == bar);
        auto *selectAll = action(bar, @"edit:", 6);
        QVERIFY(selectAll);
        QVERIFY([selectAll.target validateMenuItem:selectAll]);
        invoke(selectAll);
        QCOMPARE(object->property("selectedText").toString(), u"\u533f\u540d Music"_s);
        auto *cut = action(bar, @"edit:", 3);
        QVERIFY([cut.target validateMenuItem:cut]);
        // Read-only editors retain selection/copy but cannot mutate via system menus.
        object->setProperty("readOnly", true);
        QVERIFY(![cut.target validateMenuItem:cut]);
        invoke(cut);
        QCOMPARE(object->property("text").toString(), u"\u533f\u540d Music"_s);
        object->setProperty("readOnly", false);
        // Rebuilding the visual editor does not retain an old focus object.
        input->setParentItem(nullptr);
        object.reset();
        QVERIFY(![selectAll.target validateMenuItem:selectAll]);
        QVERIFY(action(bar, @"closeWindow:"));
        QVERIFY(action(bar, @"minimizeWindow:"));
        QVERIFY(action(bar, @"quit:"));
        QVERIFY(!window.isVisible());
        QVERIFY(yalahoo::verification::windowPolicySatisfied(window));
        menu.reset();
        QVERIFY(NSApp.mainMenu == previous);
    }
    void destroyedWindow()
    {
        auto window = std::make_unique<QQuickWindow>();
        yalahoo::verification::prepareWindow(*window);
        int opened = 0;
        yalahoo::ApplicationMenu menu(
                *window,
                [&] {
                    ++opened;
                },
                [] {
                    return true;
                });
        auto *settings = action(NSApp.mainMenu, @"openSettings:");
        window.reset();
        QVERIFY(![settings.target validateMenuItem:settings]);
        invoke(settings);
        QCOMPARE(opened, 0);
    }
};
int main(int argc, char **argv)
{
    yalahoo::verification::prepareApplication();
    QGuiApplication app(argc, argv);
    MacApplicationMenuTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "MacApplicationMenuTest.moc"
