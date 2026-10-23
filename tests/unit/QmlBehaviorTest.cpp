// Exercises production QML geometry at lifecycle boundaries without showing a window.
#include "../support/SettingsFixture.h"
#include "DetailPorts.h"
#include "PresentationController.h"
#include "OverviewController.h"
#include "LayoutMetrics.h"
#include <QTemporaryDir>
#include "DetailModel.h"
#include "LibrarySession.h"
#include "LibraryStore.h"
#include "OfflineMetadataProvider.h"
#include "VerificationWindow.h"
#include <QQuickWindow>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSignalSpy>
#include <QStyleHints>
#include <QScopeGuard>
#include <QTest>
#include <QVector2D>
#include <cmath>
#include <memory>

using namespace Qt::StringLiterals;

class QmlBehaviorTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(QmlBehaviorTest)
public:
    QmlBehaviorTest() = default;

private Q_SLOTS:
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
