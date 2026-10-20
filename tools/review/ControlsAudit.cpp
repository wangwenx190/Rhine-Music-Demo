#include <QQuickWindow>
#include "ControlsAudit.h"
#include "OffscreenSurface.h"
#include "VerificationWindow.h"
#include <QDir>
#include <QEventLoop>
#include <QInputMethodEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickTextDocument>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTest>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTimer>
#include <cmath>

using namespace Qt::StringLiterals;

using namespace yalahoo;
namespace {
QQuickItem *visual(QQuickItem *root, const QString &name)
{
    if (!root)
        return nullptr;
    if (root->objectName() == name)
        return root;
    for (auto *child : root->childItems()) {
        if (auto *found = visual(child, name))
            return found;
    }
    return nullptr;
}
}
int reviewControls(const QString &output)
{
    QDir().mkpath(output);
    QQmlEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    QQmlComponent component(&engine, QUrl(u"qrc:/qt/qml/Yalahoo/Music/ControlsReview.qml"_s));
    std::unique_ptr<QObject> object(component.create());
    auto *root = qobject_cast<QQuickItem *>(object.get());
    if (!root) {
        qWarning() << component.errors();
        return 2;
    }
    QQuickRenderControl control;
    QQuickWindow window(OffscreenSurface::Enabled ? &control : nullptr);
    verification::prepareWindow(window, true);
    window.resize(640, 480);
    root->setParentItem(window.contentItem());
    OffscreenSurface surface(window, control);
    surface.show();
    const auto render = [&] {
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        bool rendered = false;
        surface.connectFrames(&loop, [&] {
            rendered = true;
            loop.quit();
        });
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(5000);
        surface.requestFrame();
        loop.exec();
        return rendered;
    };
    const auto item = [&](const char *name) {
        return root->findChild<QQuickItem *>(name);
    };
    auto *checkBox = item("controlCheck"), *slider = item("controlSlider"),
         *choice = item("controlChoice");
    auto *field = item("controlField"), *area = item("controlArea");
    if (!checkBox || !slider || !choice || !field || !area)
        return 3;
    QJsonArray checks;
    bool passed = true;
    const auto check = [&](const QString &name, bool okay) {
        checks.append(QJsonObject{ { u"check"_s, name }, { u"passed"_s, okay } });
        passed &= okay;
        if (!okay)
            qWarning() << "Controls audit failed:" << name;
    };
    for (bool night : { false, true }) {
        root->setProperty("night", night);
        check(u"render"_s, render());
        auto *indicator = checkBox->property("indicator").value<QQuickItem *>();
        check(u"source-checkbox-16"_s, indicator && indicator->size() == QSizeF(16, 16));
        check(u"source-range-150x16"_s, slider->size() == QSizeF(150, 16));
        check(u"source-select-28"_s, choice->height() == 28);
        check(u"source-field-line-box"_s, std::abs(field->height() - 46.4) < 1e-6);
        check(u"source-button-29"_s, item("controlButton")->height() == 29);
        check(u"source-progress-3"_s, item("controlProgress")->height() == 3);
        auto *document = area->property("textDocument").value<QQuickTextDocument *>();
        bool lines = document && document->textDocument();
        if (lines)
            for (auto block = document->textDocument()->begin(); block.isValid();
                 block = block.next())
                lines &= block.blockFormat().lineHeightType() == QTextBlockFormat::FixedHeight
                        && std::abs(block.blockFormat().lineHeight() - 20.4) < 1e-6;
        check(u"source-area-line-height"_s, lines);
        check(u"own-image"_s,
              surface.grab().save(output + (night ? "/night.png"_L1 : "/day.png"_L1)));
    }
    auto *button = item("controlButton");
    button->setProperty("framed", false);
    button->setProperty("cssPixelSize", 12);
    button->setProperty("text",
                        QCoreApplication::translate("Overview", "Return to close view     V"));
    button->forceActiveFocus(Qt::TabFocusReason);
    check(u"shortcut-button-render"_s, render());
    bool textBounded = true;
    const auto inspectText = [&](auto &&self, QQuickItem *node) -> void {
        if (node->property("contentWidth").toDouble() > 0)
            textBounded &= node->width() + 1 >= node->property("contentWidth").toDouble()
                    && node->mapRectToItem(button, node->boundingRect()).right()
                            <= button->width() + 1;
        for (auto *child : node->childItems())
            self(self, child);
    };
    inspectText(inspectText, button);
    check(u"shortcut-text-bounded"_s, textBounded);
    auto *outline = visual(button, u"buttonFocusOutline"_s);
    check(u"source-keyboard-outline"_s,
          outline && outline->isVisible() && outline->position() == QPointF(-6, -6)
                  && outline->size() == button->size() + QSizeF(12, 12));
    check(u"shortcut-button-image"_s, surface.grab().save(output + "/shortcut.png"_L1));
    checkBox->forceActiveFocus();
    QTest::keyClick(&window, Qt::Key_Space);
    check(u"checkbox-keyboard"_s, checkBox->property("checked").toBool());
    slider->forceActiveFocus();
    QTest::keyClick(&window, Qt::Key_Right);
    check(u"range-keyboard"_s, std::abs(slider->property("value").toDouble() - 1.05) < 1e-6);
    choice->forceActiveFocus();
    QTest::keyClick(&window, Qt::Key_Space);
    check(u"popup-render"_s, render());
    auto *popup = choice->property("popup").value<QObject *>();
    check(u"combo-popup"_s, popup && popup->property("visible").toBool());
    check(u"popup-own-image"_s, surface.grab().save(output + "/combo.png"_L1));
    QTest::keyClick(&window, Qt::Key_Down);
    check(u"combo-keyboard-highlight"_s, choice->property("highlightedIndex").toInt() == 1);
    QTest::keyClick(&window, Qt::Key_Return);
    check(u"combo-commit"_s,
          choice->property("currentValue").toString() == "artist"_L1 && popup
                  && !popup->property("visible").toBool());
    field->forceActiveFocus();
    for (char key : "typing"_ba) {
        QTest::keyClick(&window, key);
    }
    QMetaObject::invokeMethod(field, "selectAll");
    QInputMethodEvent ime;
    ime.setCommitString(u"\u4e2d\u6587\U0001f642"_s);
    QCoreApplication::sendEvent(&window, &ime);
    check(u"unicode-ime-selection"_s,
          field->property("text").toString() == u"\u4e2d\u6587\U0001f642"_s);
    QTest::keyClick(&window, Qt::Key_F10, Qt::ShiftModifier);
    check(u"editor-menu-render"_s, render());
    auto *menu = field->findChild<QObject *>("editorMenu");
    check(u"editor-menu-open"_s, menu && menu->property("visible").toBool());
    check(u"menu-own-image"_s, surface.grab().save(output + "/editor-menu.png"_L1));
    if (menu)
        menu->setProperty("actionIndex", 2);
    QTest::keyClick(&window, Qt::Key_Down);
    QTest::keyClick(&window, Qt::Key_Return);
    check(u"editor-menu-select-all"_s,
          menu && !menu->property("visible").toBool()
                  && field->property("selectedText").toString() == u"\u4e2d\u6587\U0001f642"_s);
    field->setProperty("readOnly", true);
    QTest::keyClick(&window, Qt::Key_F10, Qt::ShiftModifier);
    check(u"readonly-menu-render"_s, render());
    auto *menuContent = menu ? menu->property("contentItem").value<QQuickItem *>() : nullptr;
    auto *cut = visual(menuContent, u"editActioncut"_s);
    auto *paste = visual(menuContent, u"editActionpaste"_s);
    check(u"readonly-menu-actions"_s, cut && paste && !cut->isEnabled() && !paste->isEnabled());
    QTest::keyClick(&window, Qt::Key_Escape);
    check(u"editor-menu-escape"_s, menu && !menu->property("visible").toBool());
    field->setProperty("readOnly", false);
    area->forceActiveFocus();
    const auto before = area->property("text").toString();
    QMetaObject::invokeMethod(area, "selectAll");
    for (char key : "undo"_ba) {
        QTest::keyClick(&window, key);
    }
    QMetaObject::invokeMethod(area, "undo");
    check(u"editor-undo-preserved"_s, area->property("text").toString() == before);
    QMetaObject::invokeMethod(area, "redo");
    check(u"editor-redo-preserved"_s, area->property("text").toString() == "undo"_L1);
    QInputMethodEvent multiline;
    multiline.setCommitString(u"\n\u65b0\u6bb5\u843d"_s);
    QCoreApplication::sendEvent(&window, &multiline);
    auto *document = area->property("textDocument").value<QQuickTextDocument *>();
    check(u"new-paragraph-line-height"_s,
          document && document->textDocument()
                  && document->textDocument()->lastBlock().blockFormat().lineHeightType()
                          == QTextBlockFormat::FixedHeight
                  && std::abs(document->textDocument()->lastBlock().blockFormat().lineHeight()
                              - 20.4)
                          < 1e-6);
    check(u"warnings"_s, warnings.isEmpty());
    check(u"window-policy"_s, verification::windowPolicySatisfied(window));
    const auto policy = verification::windowPolicy(window);
    surface.hide();
    root->setParentItem(nullptr);
    object.reset();
    QSaveFile file(output + "/report.json"_L1);
    if (!file.open(QIODevice::WriteOnly))
        return 4;
    file.write(QJsonDocument(QJsonObject{ { u"passed"_s, passed },
                                          { u"checks"_s, checks },
                                          { u"windowPolicy"_s, policy },
                                          { u"qtVersion"_s, QString::fromUtf8(QT_VERSION_STR) },
                                          { u"themeEnvironment"_s,
                                            qEnvironmentVariable("QT_QUICK_CONTROLS_STYLE") } })
                       .toJson());
    return file.commit() && passed ? 0 : 5;
}
