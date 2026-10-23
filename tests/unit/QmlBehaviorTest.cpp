// Exercises production QML geometry at lifecycle boundaries without showing a window.
#include "LayoutMetrics.h"
#include "DetailModel.h"
#include "LibrarySession.h"
#include "OfflineMetadataProvider.h"
#include "VerificationWindow.h"
#include <QQuickWindow>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSignalSpy>
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
