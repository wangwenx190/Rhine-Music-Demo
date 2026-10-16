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
    QQmlComponent component(&engine, QUrl("qrc:/qt/qml/Yalahoo/Music/ControlsReview.qml"));
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
        checks.append(QJsonObject{ { "check", name }, { "passed", okay } });
        passed &= okay;
        if (!okay)
            qWarning() << "Controls audit failed:" << name;
    };
    for (bool night : { false, true }) {
        root->setProperty("night", night);
        check("render", render());
        auto *indicator = checkBox->property("indicator").value<QQuickItem *>();
        check("source-checkbox-16", indicator && indicator->size() == QSizeF(16, 16));
        check("source-range-150x16", slider->size() == QSizeF(150, 16));
        check("source-select-28", choice->height() == 28);
        check("source-field-line-box", std::abs(field->height() - 46.4) < 1e-6);
        check("source-button-29", item("controlButton")->height() == 29);
        check("source-progress-3", item("controlProgress")->height() == 3);
        auto *document = area->property("textDocument").value<QQuickTextDocument *>();
        bool lines = document && document->textDocument();
        if (lines)
            for (auto block = document->textDocument()->begin(); block.isValid();
                 block = block.next())
                lines &= block.blockFormat().lineHeightType() == QTextBlockFormat::FixedHeight
                        && std::abs(block.blockFormat().lineHeight() - 20.4) < 1e-6;
        check("source-area-line-height", lines);
        check("own-image", surface.grab().save(output + (night ? "/night.png" : "/day.png")));
    }
    checkBox->forceActiveFocus();
    QTest::keyClick(&window, Qt::Key_Space);
    check("checkbox-keyboard", checkBox->property("checked").toBool());
    slider->forceActiveFocus();
    QTest::keyClick(&window, Qt::Key_Right);
    check("range-keyboard", std::abs(slider->property("value").toDouble() - 1.05) < 1e-6);
    choice->forceActiveFocus();
    QTest::keyClick(&window, Qt::Key_Space);
    check("popup-render", render());
    auto *popup = choice->property("popup").value<QObject *>();
    check("combo-popup", popup && popup->property("visible").toBool());
    check("popup-own-image", surface.grab().save(output + "/combo.png"));
    QTest::keyClick(&window, Qt::Key_Down);
    check("combo-keyboard-highlight", choice->property("highlightedIndex").toInt() == 1);
    QTest::keyClick(&window, Qt::Key_Return);
    check("combo-commit",
          choice->property("currentValue").toString() == "artist" && popup
                  && !popup->property("visible").toBool());
    field->forceActiveFocus();
    for (char key : QByteArray("typing")) {
        QTest::keyClick(&window, key);
    }
    QMetaObject::invokeMethod(field, "selectAll");
    QInputMethodEvent ime;
    ime.setCommitString(QStringLiteral("中文🙂"));
    QCoreApplication::sendEvent(&window, &ime);
    check("unicode-ime-selection", field->property("text").toString() == QStringLiteral("中文🙂"));
    QTest::keyClick(&window, Qt::Key_F10, Qt::ShiftModifier);
    check("editor-menu-render", render());
    auto *menu = field->findChild<QObject *>("editorMenu");
    check("editor-menu-open", menu && menu->property("visible").toBool());
    check("menu-own-image", surface.grab().save(output + "/editor-menu.png"));
    if (menu)
        menu->setProperty("actionIndex", 2);
    QTest::keyClick(&window, Qt::Key_Down);
    QTest::keyClick(&window, Qt::Key_Return);
    check("editor-menu-select-all",
          menu && !menu->property("visible").toBool()
                  && field->property("selectedText").toString() == QStringLiteral("中文🙂"));
    field->setProperty("readOnly", true);
    QTest::keyClick(&window, Qt::Key_F10, Qt::ShiftModifier);
    check("readonly-menu-render", render());
    auto *menuContent = menu ? menu->property("contentItem").value<QQuickItem *>() : nullptr;
    auto *cut = visual(menuContent, "editActioncut");
    auto *paste = visual(menuContent, "editActionpaste");
    check("readonly-menu-actions", cut && paste && !cut->isEnabled() && !paste->isEnabled());
    QTest::keyClick(&window, Qt::Key_Escape);
    check("editor-menu-escape", menu && !menu->property("visible").toBool());
    field->setProperty("readOnly", false);
    area->forceActiveFocus();
    const auto before = area->property("text").toString();
    QMetaObject::invokeMethod(area, "selectAll");
    for (char key : QByteArray("undo")) {
        QTest::keyClick(&window, key);
    }
    QMetaObject::invokeMethod(area, "undo");
    check("editor-undo-preserved", area->property("text").toString() == before);
    QMetaObject::invokeMethod(area, "redo");
    check("editor-redo-preserved", area->property("text").toString() == "undo");
    QInputMethodEvent multiline;
    multiline.setCommitString(QStringLiteral("\n新段落"));
    QCoreApplication::sendEvent(&window, &multiline);
    auto *document = area->property("textDocument").value<QQuickTextDocument *>();
    check("new-paragraph-line-height",
          document && document->textDocument()
                  && document->textDocument()->lastBlock().blockFormat().lineHeightType()
                          == QTextBlockFormat::FixedHeight
                  && std::abs(document->textDocument()->lastBlock().blockFormat().lineHeight()
                              - 20.4)
                          < 1e-6);
    check("warnings", warnings.isEmpty());
    check("window-policy", verification::windowPolicySatisfied(window));
    const auto policy = verification::windowPolicy(window);
    surface.hide();
    root->setParentItem(nullptr);
    object.reset();
    QSaveFile file(output + "/report.json");
    if (!file.open(QIODevice::WriteOnly))
        return 4;
    file.write(QJsonDocument(QJsonObject{ { "passed", passed },
                                          { "checks", checks },
                                          { "windowPolicy", policy },
                                          { "qtVersion", QT_VERSION_STR },
                                          { "themeEnvironment",
                                            qEnvironmentVariable("QT_QUICK_CONTROLS_STYLE") } })
                       .toJson());
    return file.commit() && passed ? 0 : 5;
}
