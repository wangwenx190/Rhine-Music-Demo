// Exercises production QML geometry at lifecycle boundaries without showing a window.
#include "../support/SettingsFixture.h"
#include "DetailPorts.h"
#include "PresentationController.h"
#include "OverviewController.h"
#include "OpeningController.h"
#include "SceneItem.h"
#include "LayoutMetrics.h"
#include <QTemporaryDir>
#include "DetailModel.h"
#include "LibrarySession.h"
#include "LibraryStore.h"
#include "OfflineMetadataProvider.h"
#include "VerificationWindow.h"
#include <QQuickWindow>
#include <QAccessible>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSignalSpy>
#include <QStyleHints>
#include <QScopeGuard>
#include <QTest>
#include <QVector2D>
#include <QFontMetricsF>
#include <cmath>
#include <memory>

using namespace Qt::StringLiterals;

namespace {
QList<QQuickItem *> visualItems(QQuickItem *root)
{
    QList<QQuickItem *> items{ root };
    for (qsizetype i = 0; i < items.size(); ++i)
        items.append(items[i]->childItems());
    return items;
}
}

class QmlBehaviorTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(QmlBehaviorTest)
public:
    QmlBehaviorTest() = default;

private Q_SLOTS:
    void themeOptionsExposeExclusiveAccessibleSelection()
    {
        QQmlEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Yalahoo.Music
            import "components"
            ThemeSwitch {
                tokens: DesignTokens {
                    night: false
                    reducedMotion: true
                }
                onThemeRequested: function(night) {
                    tokens.night = night;
                }
            }
        )"_ba,
                          QUrl(u"qrc:/qt/qml/Yalahoo/Music/ThemeAccessibilityTest.qml"_s));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuickItem *>(object.get());
        QVERIFY(root);
        QList<QQuickItem *> options;
        for (auto *item : visualItems(root))
            if (item->objectName() == "dayButton"_L1 || item->objectName() == "nightButton"_L1)
                options.append(item);
        QCOMPARE(options.size(), 2);
        for (bool night : { false, true, false }) {
            QVERIFY(QMetaObject::invokeMethod(root, "themeRequested", Q_ARG(bool, night)));
            for (auto *option : options) {
                auto *accessible = QAccessible::queryAccessibleInterface(option);
                QVERIFY(accessible);
                QCOMPARE(accessible->role(), QAccessible::RadioButton);
                QVERIFY(accessible->state().checkable);
                QCOMPARE(bool(accessible->state().checked),
                         night == (option->objectName() == "nightButton"_L1));
            }
        }
        QVERIFY(warnings.isEmpty());
    }
    void stageFeedbackWrapsInsideWindowMargins()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        yalahoo::test::SettingsFixture fixture(directory.path());
        yalahoo::rhi::SceneItem scene;
        scene.setAdapter(&fixture.scene);
        QQmlEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Yalahoo.Music
            import "components"
            import "panels"
            StagePanels {
                tokens: DesignTokens { reducedMotion: true }
                width: 320
                height: 720
                experimentsVisible: false
                preferencesError: "Graphics configuration unavailable: the selected output cannot allocate the requested rendering resources."
                backdropSource: Item {}
            }
        )"_ba,
                          QUrl(u"qrc:/qt/qml/Yalahoo/Music/FeedbackWidthTest.qml"_s));
        std::unique_ptr<QObject> object(component.createWithInitialProperties(
                { { u"librarySession"_s, QVariant::fromValue(&fixture.library) },
                  { u"sceneSource"_s, QVariant::fromValue(&scene) } }));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuickItem *>(object.get());
        int feedback = 0;
        for (auto *item : visualItems(root)) {
            if (!item->property("cssPixelSize").isValid()
                || item->property("text").toString()
                        != root->property("preferencesError").toString())
                continue;
            ++feedback;
            QCOMPARE(item->width(), 272);
            QVERIFY(item->property("lineCount").toInt() > 1);
            const auto rect = item->mapRectToItem(root, QRectF(QPointF(), item->size()));
            QCOMPARE(rect.left(), 24);
            QCOMPARE(rect.right(), 296);
            QVERIFY(rect.top() >= 0);
        }
        QCOMPARE(feedback, 1);
        QVERIFY(warnings.isEmpty());
    }
    void libraryStatisticsFitNarrowAndDesktopPanels()
    {
        yalahoo::LibrarySession library(nullptr);
        yalahoo::LibrarySnapshot snapshot;
        for (int i = 0; i < 1000; ++i) {
            yalahoo::Album album;
            album.id = QString::number(i);
            album.title = album.id;
            album.tracks.resize(11);
            snapshot.albums.append(album);
        }
        library.setSnapshot(std::move(snapshot));
        QQmlEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Yalahoo.Music
            import "components"
            import "panels"
            LibraryPanel {
                tokens: DesignTokens { reducedMotion: true }
                height: 720
            }
        )"_ba,
                          QUrl(u"qrc:/qt/qml/Yalahoo/Music/LibraryCountsTest.qml"_s));
        std::unique_ptr<QObject> object(component.createWithInitialProperties(
                { { u"librarySession"_s, QVariant::fromValue(&library) } }));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuickItem *>(object.get());
        for (int width : { 254, 600 }) {
            root->setWidth(width);
            int counts = 0;
            for (auto *item : visualItems(root)) {
                item->ensurePolished();
                if (item->property("cssPixelSize").toDouble() != 29)
                    continue;
                ++counts;
                QVERIFY(item->width() <= item->parentItem()->width());
                QVERIFY(item->width() > 0);
                for (auto *sibling : item->parentItem()->childItems())
                    QVERIFY(sibling->x() + sibling->width() <= item->parentItem()->width() + 1e-6);
                QVERIFY(item->property("text").toString() == u"1000"_s
                        || item->property("text").toString() == u"11000"_s
                        || item->property("text").toString() == u"0"_s);
            }
            QCOMPARE(counts, 3);
        }
        QVERIFY(warnings.isEmpty());
    }
    void searchArrowNavigationFollowsTabbedResult()
    {
        yalahoo::LibrarySession library(nullptr);
        yalahoo::LibrarySnapshot snapshot;
        for (int i = 0; i < 3; ++i) {
            yalahoo::Album album;
            album.id = QString::number(i);
            album.title = album.id;
            snapshot.albums.append(album);
        }
        library.setSnapshot(std::move(snapshot));
        QQmlEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Yalahoo.Music
            import "components"
            import "panels"
            SearchPanel {
                tokens: DesignTokens { reducedMotion: true }
                width: 500
                height: 720
            }
        )"_ba,
                          QUrl(u"qrc:/qt/qml/Yalahoo/Music/SearchTabTest.qml"_s));
        std::unique_ptr<QObject> object(component.createWithInitialProperties(
                { { u"librarySession"_s, QVariant::fromValue(&library) } }));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuickItem *>(object.get());
        QQuickWindow window;
        yalahoo::verification::prepareWindow(window);
        root->setParentItem(window.contentItem());
        auto *list = root->findChild<QQuickItem *>(u"librarySearchResults"_s);
        QVERIFY(list);
        QVERIFY(QMetaObject::invokeMethod(list, "forceLayout"));
        QQuickItem *first = nullptr, *second = nullptr;
        for (auto *item : visualItems(root)) {
            if (item->objectName() == u"librarySearchResult-0"_s)
                first = item;
            if (item->objectName() == u"librarySearchResult-1"_s)
                second = item;
        }
        QVERIFY(first && second);
        const auto previous = QGuiApplication::styleHints()->tabFocusBehavior();
        const auto restore = qScopeGuard([previous] {
            QGuiApplication::styleHints()->setTabFocusBehavior(previous);
        });
        QGuiApplication::styleHints()->setTabFocusBehavior(Qt::TabFocusAllControls);
        first->forceActiveFocus(Qt::TabFocusReason);
        QCOMPARE(list->property("currentIndex").toInt(), 0);
        QTest::keyClick(&window, Qt::Key_Tab);
        QCOMPARE(window.activeFocusItem(), second);
        QCOMPARE(list->property("currentIndex").toInt(), 1);
        QTest::keyClick(&window, Qt::Key_Down);
        QCOMPARE(list->property("currentIndex").toInt(), 2);
        QSignalSpy activated(root, SIGNAL(resultRequested(QString, QString)));
        QTest::keyClick(&window, Qt::Key_Return);
        QCOMPARE(activated.size(), 1);
        QCOMPARE(activated.first().first().toString(), u"2"_s);
        first->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(&window, Qt::Key_Up);
        auto *query = root->property("queryInput").value<QQuickItem *>();
        QCOMPARE(window.activeFocusItem(), query);
        query->forceActiveFocus();
        QTest::keyClick(&window, Qt::Key_Down);
        QCOMPARE(list->property("currentIndex").toInt(), 0);
        QCOMPARE(window.activeFocusItem(), first);
        QTest::keyClick(&window, Qt::Key_Down);
        QVERIFY(list->property("currentIndex").toInt() > 0);
        root->setParentItem(nullptr);
        QVERIFY(!window.isVisible());
        QVERIFY(warnings.isEmpty());
    }
    void openingCaptionSelectsBundledDemibold()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        yalahoo::test::SettingsFixture fixture(directory.path());
        yalahoo::OpeningController opening(&fixture.motion, &fixture.scene, &fixture.selection);
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Yalahoo.Music
            import "components"
            OpeningFilm {
                tokens: DesignTokens { reducedMotion: true }
                width: 1280
                height: 720
                markWidth: 217.6
                orbitWidth: 300
                readonly property font expectedFont: tokens.demibold
            }
        )"_ba,
                          QUrl(u"qrc:/qt/qml/Yalahoo/Music/CaptionFontTest.qml"_s));
        std::unique_ptr<QObject> object(component.createWithInitialProperties(
                { { u"controller"_s, QVariant::fromValue(&opening) } }));
        QVERIFY2(object, qPrintable(component.errorString()));
        const auto expected = object->property("expectedFont").value<QFont>();
        int captions = 0;
        for (auto *item : visualItems(qobject_cast<QQuickItem *>(object.get()))) {
            if (!item->property("cssPixelSize").isValid()
                || item->property("text").toString() != u"yalahOO!!!"_s)
                continue;
            ++captions;
            const auto actual = item->property("font").value<QFont>();
            QCOMPARE(actual.family(), expected.family());
            QCOMPARE(actual.styleName(), expected.styleName());
            QCOMPARE(actual.weight(), expected.weight());
        }
        QCOMPARE(captions, 1);
    }
    void wholeFontPreservesCssPixelSize()
    {
        QQmlEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Yalahoo.Music
            import "components"
            ArchiveText {
                tokens: DesignTokens { reducedMotion: true }
                font: tokens.regular
                text: "Collection label"
                cssPixelSize: 11.5
            }
        )"_ba,
                          QUrl(u"qrc:/qt/qml/Yalahoo/Music/FontSizeTest.qml"_s));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuickItem *>(object.get());
        QCOMPARE(root->childItems().size(), 1);
        auto *glyphs = root->childItems().first();
        QCOMPARE(glyphs->property("font").value<QFont>().pixelSize(), 12);
        root->setProperty("cssPixelSize", 14.75);
        QCOMPARE(glyphs->property("font").value<QFont>().pixelSize(), 15);
        const auto font = glyphs->property("font").value<QFont>();
        QVERIFY(std::abs(root->implicitWidth()
                         - QFontMetricsF(font).horizontalAdvance(u"Collection label"_s) * 14.75
                                 / 15)
                < 1);
        QVERIFY(warnings.isEmpty());
    }
    void scrollbarMinimumUsesAvailableTrack()
    {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Yalahoo.Music
            import "components"
            ArchiveScrollBar {
                tokens: DesignTokens { reducedMotion: true }
                width: 100; height: 100
                size: .001
            }
        )"_ba,
                          QUrl(u"qrc:/qt/qml/Yalahoo/Music/ScrollbarSizeTest.qml"_s));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        for (const auto orientation : { Qt::Horizontal, Qt::Vertical }) {
            object->setProperty("orientation", orientation);
            auto *handle = object->property("contentItem").value<QQuickItem *>();
            QVERIFY(handle);
            QTRY_VERIFY(
                    std::abs((orientation == Qt::Horizontal ? handle->width() : handle->height())
                             - 20)
                    < 1e-6);
            object->setProperty(orientation == Qt::Horizontal ? "width" : "height", 18);
            QTRY_VERIFY(
                    std::abs((orientation == Qt::Horizontal ? handle->width() : handle->height())
                             - 14)
                    < 1e-6);
            object->setProperty("width", 100);
            object->setProperty("height", 100);
        }
    }
    void editMenuMixesTabAndArrowNavigation()
    {
        QQmlEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Yalahoo.Music
            import "components"
            Item {
                width: 300; height: 200
                DesignTokens { id: design; reducedMotion: true }
                ArchiveTextField { id: field; objectName: "editor"; tokens: design; text: "abcdef"; readOnly: true }
                ArchiveEditMenu { tokens: design; editor: field }
            }
        )"_ba,
                          QUrl(u"qrc:/qt/qml/Yalahoo/Music/EditMenuNavigationTest.qml"_s));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuickItem *>(object.get());
        QQuickWindow window;
        yalahoo::verification::prepareWindow(window);
        root->setParentItem(window.contentItem());
        auto *editor = root->findChild<QObject *>(u"editor"_s);
        auto *menu = root->findChild<QObject *>(u"editorMenu"_s);
        QVERIFY(editor && menu);
        QVERIFY(QMetaObject::invokeMethod(editor, "select", Q_ARG(int, 0), Q_ARG(int, 3)));
        const auto previous = QGuiApplication::styleHints()->tabFocusBehavior();
        const auto restore = qScopeGuard([previous] {
            QGuiApplication::styleHints()->setTabFocusBehavior(previous);
        });
        QGuiApplication::styleHints()->setTabFocusBehavior(Qt::TabFocusAllControls);
        QVERIFY(QMetaObject::invokeMethod(menu, "open"));
        QTRY_COMPARE(menu->property("actionIndex").toInt(), 1);
        QTest::keyClick(&window, Qt::Key_Tab);
        QCOMPARE(menu->property("actionIndex").toInt(), 3);
        QTest::keyClick(&window, Qt::Key_Down);
        QCOMPARE(menu->property("actionIndex").toInt(), 1);
        QTest::keyClick(&window, Qt::Key_Up);
        QCOMPARE(menu->property("actionIndex").toInt(), 3);
        QTest::keyClick(&window, Qt::Key_Backtab, Qt::ShiftModifier);
        QCOMPARE(menu->property("actionIndex").toInt(), 1);
        QTest::keyClick(&window, Qt::Key_Up);
        QCOMPARE(menu->property("actionIndex").toInt(), 3);
        QVERIFY(QMetaObject::invokeMethod(menu, "close"));
        root->setParentItem(nullptr);
        QVERIFY(!window.isVisible());
        QVERIFY(warnings.isEmpty());
    }
    void panelHeadingWrapsWithinCloseButtonSpace()
    {
        QQmlEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Yalahoo.Music
            import "components"
            import "panels"
            PanelHeading {
                tokens: DesignTokens { reducedMotion: true }
                title: "A long music library settings title"
                subtitle: "COLLECTION / SETTINGS / PREFERENCES"
                property int expectedWrap: Text.WrapAnywhere
            }
        )"_ba,
                          QUrl(u"qrc:/qt/qml/Yalahoo/Music/HeadingWidthTest.qml"_s));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuickItem *>(object.get());
        for (const auto width : { 254, 600 }) {
            root->setWidth(width);
            const auto items = visualItems(root);
            for (auto it = items.crbegin(); it != items.crend(); ++it)
                (*it)->ensurePolished();
            int copies = 0;
            for (auto *item : visualItems(root)) {
                const auto text = item->property("text").toString();
                if (!item->property("cssPixelSize").isValid()
                    || (text != root->property("title").toString()
                        && text != root->property("subtitle").toString()))
                    continue;
                ++copies;
                const auto rect = item->mapRectToItem(root, QRectF(QPointF(), item->size()));
                QVERIFY(rect.right() <= width - 32);
                QCOMPARE(item->property("wrapMode"), root->property("expectedWrap"));
                QVERIFY(rect.bottom() <= root->implicitHeight());
            }
            QCOMPARE(copies, 2);
        }
        QVERIFY(warnings.isEmpty());
    }
    void genreKeyboardFocusRemainsVisibleInsideTheClippedViewport()
    {
        QQmlEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Yalahoo.Music
            import "panels"
            SearchFilters {
                id: filters
                width: 300
                tokens: DesignTokens { reducedMotion: true }
                genreRows: GenreRowsModel {
                    width: filters.genreContentWidth
                    genres: [{id: "jazz", name: "Jazz"}]
                }
                queryText: ""
                genreId: ""
                viewportHeight: 108
            }
        )"_ba,
                          QUrl(u"qrc:/qt/qml/Yalahoo/Music/GenreFocusTest.qml"_s));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuickItem *>(object.get());
        QQuickWindow window;
        yalahoo::verification::prepareWindow(window);
        window.resize(400, 300);
        root->setParentItem(window.contentItem());
        auto *list = root->findChild<QQuickItem *>(u"libraryGenreFilters"_s);
        QVERIFY(list);
        QVERIFY(QMetaObject::invokeMethod(list, "forceLayout"));
        const auto find = [](auto &&self, QQuickItem *item, const QString &name) -> QQuickItem * {
            if (item->objectName() == name)
                return item;
            for (auto *child : item->childItems()) {
                if (auto *match = self(self, child, name))
                    return match;
            }
            return nullptr;
        };
        QQuickItem *all = nullptr, *jazz = nullptr;
        QTRY_VERIFY((all = find(find, list, u"genreFilter-"_s)));
        QTRY_VERIFY((jazz = find(find, list, u"genreFilter-jazz"_s)));
        auto *outline = jazz->findChild<QQuickItem *>(u"genreFocusOutline"_s);
        QVERIFY(outline);
        const auto tabBehavior = QGuiApplication::styleHints()->tabFocusBehavior();
        const auto restore = qScopeGuard([&] {
            QGuiApplication::styleHints()->setTabFocusBehavior(tabBehavior);
        });
        QGuiApplication::styleHints()->setTabFocusBehavior(Qt::TabFocusAllControls);
        all->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(&window, Qt::Key_Tab);
        QCOMPARE(window.activeFocusItem(), jazz);
        QVERIFY(outline->isVisible());
        QVERIFY(all->property("checked").toBool());
        QVERIFY(!jazz->property("checked").toBool());
        QVERIFY(list->clip());
        QCOMPARE(outline->position(), QPointF(-6, -6));
        QCOMPARE(outline->size(), jazz->size() + QSizeF(12, 12));
        const QRectF viewport(QPointF(), list->size());
        const auto focusRect = outline->mapRectToItem(list, QRectF(QPointF(), outline->size()));
        // Item transforms can place a mathematically coincident edge a few ULPs outside.
        const auto within = [](qreal begin, qreal end, qreal extent) {
            return (begin >= 0 || qFuzzyIsNull(begin))
                    && (end <= extent || qFuzzyCompare(end, extent));
        };
        QVERIFY2(within(focusRect.left(), focusRect.right(), viewport.width())
                         && within(focusRect.top(), focusRect.bottom(), viewport.height()),
                 qPrintable(
                         u"Viewport %1x%2, focus %3,%4 %5x%6, content %7,%8"_s.arg(viewport.width())
                                 .arg(viewport.height())
                                 .arg(focusRect.x())
                                 .arg(focusRect.y())
                                 .arg(focusRect.width())
                                 .arg(focusRect.height())
                                 .arg(list->property("contentX").toDouble())
                                 .arg(list->property("contentY").toDouble())));
        QCOMPARE(outline->property("border").value<QObject *>()->property("width").toInt(), 2);
        QSignalSpy requested(root, SIGNAL(genreRequested(QString)));
        QTest::keyClick(&window, Qt::Key_Space);
        QCOMPARE(requested.size(), 1);
        QCOMPARE(requested.first().first().toString(), u"jazz"_s);
        all->forceActiveFocus(Qt::OtherFocusReason);
        jazz->forceActiveFocus(Qt::MouseFocusReason);
        QVERIFY(!outline->isVisible());
        root->setParentItem(nullptr);
        QVERIFY(!window.isVisible());
        QVERIFY(warnings.isEmpty());
    }
    void tooltipFollowsAncestorMovementAwayFromWindowEdges()
    {
        QQmlEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Yalahoo.Music
            import "components"
            Item {
                width: 400
                height: 300
                Item {
                    objectName: "ancestor"
                    x: 350
                    y: 270
                    Item {
                        id: host
                        width: 40
                        height: 20
                        ArchiveToolTip {
                            objectName: "tip"
                            parent: host
                            tokens: DesignTokens { reducedMotion: true }
                            text: "Tooltip follows its host"
                        }
                    }
                }
            }
        )"_ba,
                          QUrl(u"qrc:/qt/qml/Yalahoo/Music/TooltipPositionTest.qml"_s));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuickItem *>(object.get());
        auto *ancestor = root->findChild<QQuickItem *>(u"ancestor"_s);
        auto *tip = root->findChild<QObject *>(u"tip"_s);
        QVERIFY(ancestor && tip);
        QQuickWindow window;
        yalahoo::verification::prepareWindow(window);
        window.resize(400, 300);
        root->setParentItem(window.contentItem());
        QVERIFY(QMetaObject::invokeMethod(tip, "open"));
        QTRY_VERIFY(tip->property("visible").toBool());
        auto *content = tip->property("contentItem").value<QQuickItem *>();
        QVERIFY(content && content->parentItem());
        auto *popup = content->parentItem();
        const auto position = [&] {
            return popup->mapToItem(window.contentItem(), QPointF());
        };
        QTRY_VERIFY2(position().x() + popup->width() <= 392.01,
                     qPrintable(u"Popup x=%1 width=%2 window=%3"_s.arg(position().x())
                                        .arg(popup->width())
                                        .arg(window.width())));
        QTRY_VERIFY(position().y() + popup->height() <= 292.01);
        ancestor->setPosition({ 100, 80 });
        QTRY_VERIFY2(std::abs(position().y() - 104) < .01,
                     qPrintable(u"Popup y=%1, expected 104"_s.arg(position().y())));
        QTRY_VERIFY(std::abs(position().x() - (120 - popup->width() / 2)) < .01);
        window.resize(450, 350);
        ancestor->setPosition({ 390, 315 });
        QTRY_VERIFY(position().x() + popup->width() <= 442.01);
        QTRY_VERIFY(position().y() + popup->height() <= 342.01);
        QVERIFY(QMetaObject::invokeMethod(tip, "close"));
        QVERIFY(!window.isVisible());
        QVERIFY(warnings.isEmpty());
    }
    void transportKeyboardFocusIsVisibleAndLocates()
    {
        QQmlEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Yalahoo.Music
            import "components"
            Item {
                width: 400
                height: 100
                ArchiveButton {
                    objectName: "beforeTitle"
                    tokens: DesignTokens { reducedMotion: true }
                    text: "Before"
                }
                TransportTitle {
                    objectName: "titleButton"
                    x: 100
                    tokens: DesignTokens { reducedMotion: true }
                    title: "Anonymous song"
                }
            }
        )"_ba,
                          QUrl(u"qrc:/qt/qml/Yalahoo/Music/TransportFocusTest.qml"_s));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuickItem *>(object.get());
        auto *before = root->findChild<QQuickItem *>(u"beforeTitle"_s);
        auto *button = root->findChild<QQuickItem *>(u"titleButton"_s);
        auto *outline = root->findChild<QQuickItem *>(u"transportTitleFocusOutline"_s);
        QVERIFY(before && button && outline);
        QQuickWindow window;
        yalahoo::verification::prepareWindow(window);
        root->setParentItem(window.contentItem());
        QTRY_VERIFY(button->isEnabled() && button->width() > 0);
        const auto tabBehavior = QGuiApplication::styleHints()->tabFocusBehavior();
        const auto restoreTabBehavior = qScopeGuard([&] {
            QGuiApplication::styleHints()->setTabFocusBehavior(tabBehavior);
        });
        QGuiApplication::styleHints()->setTabFocusBehavior(Qt::TabFocusAllControls);
        before->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(&window, Qt::Key_Tab);
        QCOMPARE(window.activeFocusItem(), button);
        QVERIFY(outline->isVisible());
        QVERIFY(!button->clip());
        QVERIFY(button->property("contentItem").value<QQuickItem *>()->clip());
        QCOMPARE(outline->position(), QPointF(-6, -6));
        QCOMPARE(outline->size(), button->size() + QSizeF(12, 12));
        QCOMPARE(outline->property("border").value<QObject *>()->property("width").toInt(), 2);
        QSignalSpy located(button, SIGNAL(locateRequested()));
        QTest::keyClick(&window, Qt::Key_Space);
        QCOMPARE(located.size(), 1);
        QTest::keyClick(&window, Qt::Key_Backtab, Qt::ShiftModifier);
        QCOMPARE(window.activeFocusItem(), before);
        QVERIFY(!outline->isVisible());
        button->forceActiveFocus(Qt::MouseFocusReason);
        QVERIFY(!outline->isVisible());
        button->setProperty("title", QString());
        QVERIFY(!button->isEnabled());
        QTest::keyClick(&window, Qt::Key_Space);
        QCOMPARE(located.size(), 1);
        root->setParentItem(nullptr);
        QVERIFY(!window.isVisible());
        QVERIFY(warnings.isEmpty());
    }
    void metadataDraftSurvivesImmediateConfigurationSaves()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        yalahoo::LibraryStore store(directory.path());
        yalahoo::OfflineMetadataProvider provider;
        yalahoo::LibrarySession library(&provider);
        connect(&store, &yalahoo::LibraryStore::metadataConfigurationChanged, &library,
                &yalahoo::LibrarySession::adoptMetadataConfiguration);
        store.load();
        QTRY_VERIFY(!store.busy());
        QVERIFY(store.setMetadataConfiguration(
                { { u"proxyEnabled"_s, true },
                  { u"proxyMode"_s, u"manual"_s },
                  { u"proxyUrl"_s, u"http://old.invalid:8080"_s },
                  { u"musicBrainzContact"_s, u"old@example.invalid"_s } }));
        QTRY_VERIFY(!store.busy());
        QQmlEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Yalahoo.Music
            import "components"
            import "panels"
            MetadataSettings {
                tokens: DesignTokens { reducedMotion: true }
                width: 500
                expanded: true
            }
        )"_ba,
                          QUrl(u"qrc:/qt/qml/Yalahoo/Music/MetadataDraftTest.qml"_s));
        std::unique_ptr<QObject> object(component.createWithInitialProperties(
                { { u"librarySession"_s, QVariant::fromValue(&library) },
                  { u"store"_s, QVariant::fromValue(&store) } }));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuickItem *>(object.get());
        auto *proxy = root->findChild<QQuickItem *>(u"settingsMetadataProxy"_s);
        auto *contact = root->findChild<QQuickItem *>(u"settingsMetadataContact"_s);
        auto *save = root->findChild<QQuickItem *>(u"settingsMetadataSave"_s);
        QVERIFY(proxy && contact && save);
        QQuickWindow window;
        yalahoo::verification::prepareWindow(window);
        root->setParentItem(window.contentItem());
        QCOMPARE(proxy->property("text").toString(), u"http://old.invalid:8080"_s);
        QCOMPARE(contact->property("text").toString(), u"old@example.invalid"_s);
        for (auto *field : { proxy, contact }) {
            field->forceActiveFocus(Qt::TabFocusReason);
            QVERIFY(QMetaObject::invokeMethod(field, "selectAll"));
            QTest::keyClick(&window, Qt::Key_Backspace);
            const auto text =
                    field == proxy ? "http://new.invalid:8888"_ba : "new@example.invalid"_ba;
            for (char key : text)
                QTest::keyClick(&window, key);
        }
        for (const auto &patch : { QVariantMap{ { u"networkEnabled"_s, true } },
                                   QVariantMap{ { u"proxyEnabled"_s, false } },
                                   QVariantMap{ { u"proxyMode"_s, u"system"_s } },
                                   QVariantMap{ { u"onlineEnabled"_s, true } } }) {
            QVERIFY(store.setMetadataConfiguration(patch));
            QTRY_VERIFY(!store.busy());
            QCOMPARE(proxy->property("text").toString(), u"http://new.invalid:8888"_s);
            QCOMPARE(contact->property("text").toString(), u"new@example.invalid"_s);
        }
        QVERIFY(QMetaObject::invokeMethod(save, "clicked"));
        QTRY_VERIFY(!store.busy());
        yalahoo::LibraryStore restored(directory.path());
        restored.load();
        QTRY_VERIFY(!restored.busy());
        const auto configuration =
                restored.snapshot()->fields[u"metadataConfiguration"_s].toObject();
        QCOMPARE(configuration[u"proxyUrl"_s].toString(), u"http://new.invalid:8888"_s);
        QCOMPARE(configuration[u"musicBrainzContact"_s].toString(), u"new@example.invalid"_s);
        // Once accepted, untouched fields follow later external configuration changes again.
        QVERIFY(store.setMetadataConfiguration(
                { { u"musicBrainzContact"_s, u"external@example.invalid"_s } }));
        QCOMPARE(contact->property("text").toString(), u"external@example.invalid"_s);
        root->setParentItem(nullptr);
        QVERIFY(!window.isVisible());
        QVERIFY(warnings.isEmpty());
    }
    void detailNavigationTracksArrayMode()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        yalahoo::test::SettingsFixture fixture(directory.path());
        yalahoo::LibrarySnapshot snapshot;
        for (const auto &id : { u"first"_s, u"last"_s }) {
            yalahoo::Album album;
            album.id = id;
            album.title = id;
            snapshot.albums.append(album);
        }
        fixture.library.setSnapshot(snapshot);
        fixture.selection.setColumns(fixture.library.albums()->order().columns,
                                     fixture.library.snapshot());
        fixture.scene.setSelection(&fixture.selection);
        yalahoo::PresentationController presentation(
                fixture.motion.clock(),
                yalahoo::detailPorts(fixture.scene, fixture.selection, fixture.library));
        QQmlEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Yalahoo.Music
            import "components"
            DetailAlbumInfo {
                width: 500
                viewportWidth: 1280
                tokens: DesignTokens { reducedMotion: true }
                album: ({title: "Album", artist: "Artist"})
            }
        )"_ba,
                          QUrl(u"qrc:/qt/qml/Yalahoo/Music/NavigationBindingTest.qml"_s));
        std::unique_ptr<QObject> object(component.createWithInitialProperties(
                { { u"presentation"_s, QVariant::fromValue(&presentation) } }));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *previous = object->findChild<QQuickItem *>(u"detailPrevious"_s);
        auto *next = object->findChild<QQuickItem *>(u"detailNext"_s);
        QVERIFY(previous && next);
        QSignalSpy changes(&presentation, &yalahoo::PresentationController::navigationChanged);
        connect(&presentation, &yalahoo::PresentationController::prepared, &presentation, [&] {
            presentation.layoutReady(presentation.layoutRevision());
        });
        fixture.motion.setReducedMotion(true);
        fixture.scene.setReducedMotion(true);
        presentation.setReducedMotion(true);
        fixture.motion.clock()->setActive(true);
        fixture.motion.clock()->advance(0);
        for (const auto &id : { u"first"_s, u"last"_s }) {
            presentation.select(id, true);
            for (int frame = 0; frame < 2400 && !presentation.detailEnabled(); ++frame)
                fixture.motion.clock()->advance(1. / 120);
            QVERIFY(presentation.detailEnabled());
            QCOMPARE(fixture.selection.albumId(), id);
            QVERIFY(fixture.settings.update(u"arrayMode"_s, u"realistic"_s));
            QCOMPARE(previous->isEnabled(), id != u"first"_s);
            QCOMPARE(next->isEnabled(), id != u"last"_s);
            changes.clear();
            QVERIFY(fixture.settings.update(u"arrayMode"_s, u"filled"_s));
            QVERIFY(previous->isEnabled());
            QVERIFY(next->isEnabled());
            QCOMPARE(changes.size(), 1);
            QVERIFY(fixture.settings.update(u"arrayMode"_s, u"filled"_s));
            QCOMPARE(changes.size(), 1);
            QVERIFY(fixture.settings.update(u"arrayMode"_s, u"realistic"_s));
            QCOMPARE(previous->isEnabled(), id != u"first"_s);
            QCOMPARE(next->isEnabled(), id != u"last"_s);
        }
        QVERIFY(warnings.isEmpty());
    }
    void overviewColumnKindFollowsLiveAndRestoredSorting()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QStringList modes{ u"genre"_s, u"artist"_s, u"album"_s };
        const QStringList names{ u"\u6d41\u6d3e"_s, u"\u827a\u672f\u5bb6"_s,
                                 u"\u4e13\u8f91\u6807\u9898"_s };
        for (int saved = 0; saved < modes.size(); ++saved) {
            {
                yalahoo::test::SettingsFixture fixture(directory.path());
                QVERIFY(fixture.settings.update(u"sortMode"_s, modes[saved]));
            }
            yalahoo::test::SettingsFixture fixture(directory.path());
            yalahoo::OverviewController overview(&fixture.motion);
            QQmlEngine engine;
            QSignalSpy warnings(&engine, &QQmlEngine::warnings);
            QQmlComponent component(&engine);
            component.setData(R"(
                import QtQuick
                import Yalahoo.Music
                import "pages"
                MusicStage {
                    width: 1280
                    height: 720
                }
            )"_ba,
                              QUrl(u"qrc:/qt/qml/Yalahoo/Music/OverviewSortingTest.qml"_s));
            std::unique_ptr<QObject> object(component.createWithInitialProperties(
                    { { u"librarySession"_s, QVariant::fromValue(&fixture.library) },
                      { u"motionController"_s, QVariant::fromValue(&fixture.motion) },
                      { u"sceneAdapter"_s, QVariant::fromValue(&fixture.scene) },
                      { u"settingsController"_s, QVariant::fromValue(&fixture.settings) },
                      { u"overviewController"_s, QVariant::fromValue(&overview) } }));
            QVERIFY2(object, qPrintable(component.errorString()));
            auto *page = object->findChild<QQuickItem *>(u"collectionOverview"_s);
            QVERIFY(page);
            for (const auto &entry : { std::pair{ u"overviewPrevious"_s, "Previous column" },
                                       std::pair{ u"overviewNext"_s, "Next column" } }) {
                auto *button = page->findChild<QQuickItem *>(entry.first);
                QVERIFY(button);
                auto *accessible = QAccessible::queryAccessibleInterface(button);
                QVERIFY(accessible);
                QCOMPARE(accessible->text(QAccessible::Name),
                         QCoreApplication::translate("Overview", entry.second));
            }
            QCOMPARE(page->property("columnKind").toString(), names[saved]);
            for (int mode = 0; mode < modes.size(); ++mode) {
                QVERIFY(fixture.settings.update(u"sortMode"_s, modes[mode]));
                QCOMPARE(page->property("columnKind").toString(), names[mode]);
                QVERIFY(fixture.settings.update(u"sortMode"_s, modes[mode]));
                QCOMPARE(page->property("columnKind").toString(), names[mode]);
            }
            QVERIFY(warnings.isEmpty());
        }
    }
    void trackPressCannotActivateReusedIdentity_data()
    {
        QTest::addColumn<bool>("changeTrack");
        QTest::addColumn<bool>("changeAlbum");
        QTest::newRow("same-row") << false << false;
        QTest::newRow("reused-track") << true << false;
        QTest::newRow("same-track-id-in-other-album") << false << true;
    }
    void trackPressCannotActivateReusedIdentity()
    {
        QFETCH(bool, changeTrack);
        QFETCH(bool, changeAlbum);
        yalahoo::OfflineMetadataProvider provider;
        yalahoo::LibrarySession library(&provider);
        yalahoo::LibrarySnapshot snapshot;
        for (const auto &id : { u"album-A"_s, u"album-B"_s }) {
            yalahoo::Album album;
            album.id = id;
            album.title = id;
            snapshot.albums.append(album);
        }
        library.setSnapshot(snapshot);
        QVERIFY(library.navigate(u"album-A"_s));
        yalahoo::DetailModel model;
        model.prepare(library);
        QQmlEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Yalahoo.Music
            import "components"
            TrackRow {
                tokens: DesignTokens { reducedMotion: true }
                index: 0
                trackId: "track-A"
                number: "1"
                trackTitle: "Track A"
                artist: "Artist"
                format: "WAV"
                duration: "1:00"
                disc: 1
                discHeading: false
                viewportWidth: 1280
                contentWidth: 300
                columnGap: 9
                numberWidth: 23
                formatWidth: 40
                durationWidth: 38
            }
        )"_ba,
                          QUrl(u"qrc:/qt/qml/Yalahoo/Music/TrackPressTest.qml"_s));
        std::unique_ptr<QObject> object(component.createWithInitialProperties(
                { { u"detailModel"_s, QVariant::fromValue(&model) } }));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *row = qobject_cast<QQuickItem *>(object.get());
        QQuickWindow window;
        yalahoo::verification::prepareWindow(window);
        window.resize(400, 200);
        row->setParentItem(window.contentItem());
        QSignalSpy activated(row, SIGNAL(trackRequested(QString)));
        const QPoint point(150, int(row->height() / 2));
        QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, point);
        if (changeTrack)
            row->setProperty("trackId", u"track-B"_s);
        if (changeAlbum) {
            QVERIFY(library.navigate(u"album-B"_s));
            model.prepare(library);
        }
        QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, point);
        QCOMPARE(activated.size(), changeTrack || changeAlbum ? 0 : 1);
        // A later fresh press must work after reuse.
        activated.clear();
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, point);
        QCOMPARE(activated.size(), 1);
        QCOMPARE(activated.first().first().toString(), changeTrack ? u"track-B"_s : u"track-A"_s);
        auto *accessible = QAccessible::queryAccessibleInterface(row);
        QVERIFY(accessible);
        QCOMPARE(accessible->role(), QAccessible::ListItem);
        auto *actions = accessible->actionInterface();
        QVERIFY(actions);
        QVERIFY(actions->actionNames().contains(QAccessibleActionInterface::pressAction()));
        QCOMPARE(accessible->childCount(), 0);
        activated.clear();
        actions->doAction(QAccessibleActionInterface::pressAction());
        QCOMPARE(activated.size(), 1);
        QCOMPARE(activated.first().first().toString(), row->property("trackId").toString());
        auto offline = *library.snapshot();
        for (auto &album : offline.albums)
            album.offline = true;
        library.setSnapshot(offline);
        model.prepare(library);
        activated.clear();
        QVERIFY(accessible->state().disabled);
        actions->doAction(QAccessibleActionInterface::pressAction());
        QCOMPARE(activated.size(), 0);
        row->setParentItem(nullptr);
        QVERIFY(!window.isVisible());
        QVERIFY(warnings.isEmpty());
    }
    void zeroSizeBlurHasFiniteSamplingAxes()
    {
        QQmlEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Yalahoo.Music
            import "components"
            Item {
                Item { id: scene }
                BackdropBlur {
                    anchors.fill: parent
                    sceneSource: scene
                    sceneX: 0
                    sceneY: 0
                    blurRadius: 0
                    horizontalPass: horizontal
                    BackdropBlurPass {
                        id: horizontal
                        objectName: "horizontal"
                        backdrop: parent
                        horizontal: true
                    }
                    BackdropBlurPass {
                        objectName: "vertical"
                        backdrop: parent
                    }
                }
            }
        )"_ba,
                          QUrl(u"qrc:/qt/qml/Yalahoo/Music/BlurBoundaryTest.qml"_s));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuickItem *>(object.get());
        auto *horizontal = root->findChild<QObject *>("horizontal");
        auto *vertical = root->findChild<QObject *>("vertical");
        QVERIFY(horizontal);
        QVERIFY(vertical);
        QCOMPARE(horizontal->property("sampleStep").value<QVector2D>(), QVector2D(1, 0));
        QCOMPARE(vertical->property("sampleStep").value<QVector2D>(), QVector2D(0, 1));
        root->setSize(QSizeF(240, 128));
        QCOMPARE(horizontal->property("sampleStep").value<QVector2D>(), QVector2D(1.f / 240, 0));
        QCOMPARE(vertical->property("sampleStep").value<QVector2D>(), QVector2D(0, 1.f / 128));
        root->setSize(QSizeF(0, 0));
        QCOMPARE(horizontal->property("sampleStep").value<QVector2D>(), QVector2D(1, 0));
        QCOMPARE(vertical->property("sampleStep").value<QVector2D>(), QVector2D(0, 1));
        QVERIFY(warnings.isEmpty());
    }
    void zeroHeightVeilHasFiniteGradientStops()
    {
        yalahoo::LayoutMetrics metrics;
        metrics.setViewport(390, 844);
        QQmlEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Yalahoo.Music
            import "components"
            ReadingVeil {
                tokens: DesignTokens { reducedMotion: true }
                width: 390
                height: 0
            }
        )"_ba,
                          QUrl(u"qrc:/qt/qml/Yalahoo/Music/VeilBoundaryTest.qml"_s));
        std::unique_ptr<QObject> object(component.createWithInitialProperties(
                { { u"metrics"_s, QVariant::fromValue(&metrics) } }));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuickItem *>(object.get());
        const auto verifyStops = [&] {
            int count = 0;
            for (auto *stop : root->findChildren<QObject *>()) {
                if (QByteArray(stop->metaObject()->className()) != "QQuickGradientStop"_ba)
                    continue;
                ++count;
                const double value = stop->property("position").toDouble();
                if (!std::isfinite(value) || value < 0 || value > 1)
                    return false;
            }
            return count >= 3;
        };
        QVERIFY(verifyStops());
        root->setHeight(844);
        root->setProperty("calloutTop", 400.);
        root->setProperty("calloutHeight", 200.);
        QVERIFY(verifyStops());
        root->setHeight(0);
        QVERIFY(verifyStops());
        QVERIFY(warnings.isEmpty());
    }
    void stackedRangeUsesFullWidth_data()
    {
        QTest::addColumn<double>("width");
        QTest::newRow("compact") << 210.;
        QTest::newRow("desktop") << 300.;
    }
    void stackedRangeUsesFullWidth()
    {
        QFETCH(double, width);
        QQmlEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import Yalahoo.Music
            import "components"
            import "panels"
            SettingRange {
                objectName: "range"
                tokens: DesignTokens { reducedMotion: true }
                label: "Range"
                value: 0.5
                stacked: true
            }
        )"_ba,
                          QUrl(u"qrc:/qt/qml/Yalahoo/Music/RangeBoundaryTest.qml"_s));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuickItem *>(object.get());
        root->setWidth(width);
        auto *slider = root->findChild<QQuickItem *>("rangeControl");
        QVERIFY(slider);
        QCOMPARE(slider->width(), width);
        QCOMPARE(slider->height(), 20.);
        root->setProperty("suffix", u" seconds"_s);
        root->setProperty("value", 2.4);
        QCOMPARE(slider->width(), width);
        root->setProperty("stacked", false);
        QCOMPARE(slider->width(), std::min(150., width * .48));
        QCOMPARE(slider->height(), 16.);
        QVERIFY(warnings.isEmpty());
    }
};

int testQmlBehavior()
{
    QmlBehaviorTest test;
    return QTest::qExec(&test, QStringList{ u"qml-behavior"_s });
}
#include "QmlBehaviorTest.moc"
