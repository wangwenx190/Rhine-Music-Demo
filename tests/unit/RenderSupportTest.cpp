#include "CaptureMailbox.h"
#include "ReviewSupport.h"
#include "VerificationWindow.h"
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QQuickItem>
#include <QVector2D>
#include <QtTest>

using namespace Qt::StringLiterals;
using namespace yalahoo;
using namespace yalahoo::review;

class RenderSupportTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(RenderSupportTest)
public:
    RenderSupportTest() = default;

private Q_SLOTS:
    void dispatchUsesStageIdentityAndPreservesUnknownState()
    {
        struct Context final
        {
            int calls = 0;
        } context;
        static constexpr StageRoute<Context> routes[] = { { 17,
                                                            [](Context &c) {
                                                                c.calls += 1;
                                                            } },
                                                          { 42, [](Context &c) {
                                                               c.calls += 10;
                                                           } } };
        QVERIFY(dispatchStage(routes, 42, context));
        QCOMPARE(context.calls, 10);
        QVERIFY(!dispatchStage(routes, -1, context));
        QCOMPARE(context.calls, 10);
        QVERIFY(dispatchStage(routes, 17, context));
        QCOMPARE(context.calls, 11);
    }
    void captureRejectsStaleAndUnwatchedCompletion()
    {
        QObject receiver;
        QEventLoop loop;
        auto mailbox = std::make_shared<CaptureMailbox>();
        mailbox->loop = &loop;
        mailbox->request = 2;
        rhi::Capture capture;
        capture.request = 1;
        queueCapture(mailbox, &receiver, capture);
        QCoreApplication::sendPostedEvents(&receiver, QEvent::MetaCall);
        QVERIFY(!mailbox->capture);
        capture.request = 2;
        queueCapture(mailbox, &receiver, capture);
        // A new request supersedes even a result already queued to the GUI.
        mailbox->request = 3;
        QCoreApplication::sendPostedEvents(&receiver, QEvent::MetaCall);
        QVERIFY(!mailbox->capture);
        capture.request = 3;
        queueCapture(mailbox, &receiver, capture);
        QCoreApplication::sendPostedEvents(&receiver, QEvent::MetaCall);
        QVERIFY(mailbox->capture);
        QCOMPARE(mailbox->capture->request, quint64(3));
        mailbox->capture.reset();
        mailbox->loop = nullptr;
        queueCapture(mailbox, &receiver, capture);
        QCoreApplication::sendPostedEvents(&receiver, QEvent::MetaCall);
        QVERIFY(!mailbox->capture);
    }
    void captureDoesNotOutliveReceiver()
    {
        auto mailbox = std::make_shared<CaptureMailbox>();
        QEventLoop loop;
        mailbox->loop = &loop;
        auto receiver = std::make_unique<QObject>();
        QPointer<QObject> guard(receiver.get());
        queueCapture(mailbox, guard, { });
        receiver.reset();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
        QVERIFY(!mailbox->capture);
        queueCapture(mailbox, guard, { });
        QVERIFY(!mailbox->capture);
    }
    void lightingSnapshotFollowsThemeWithoutChangingSubmission()
    {
        SelectionLighting lighting;
        lighting.setReducedMotion(true);
        rhi::Input input;
        input.request = 17;
        input.setLighting(lighting);
        QCOMPARE(input.warmth, 1.f);
        QCOMPARE(input.lightIntensities, QVector3D(.45f, .32f, .045f));
        QCOMPARE(input.exposure, 1.05f);
        QCOMPARE(input.scatterStrength, 1.f);
        lighting.setNight(true);
        input.setLighting(lighting);
        QCOMPARE(input.warmth, 0.f);
        QCOMPARE(input.environmentIntensity, .16f);
        QCOMPARE(input.lightIntensities, QVector3D(.30f, .21f, .045f));
        QCOMPARE(input.exposure, 1.08f);
        QCOMPARE(input.printAmbient, .12f);
        QCOMPARE(input.scatterStrength, .72f);
        QCOMPARE(input.spotIntensity, 130.f * 64);
        QCOMPARE(input.request, quint64(17));
    }
    void backdropGeometry_data()
    {
        QTest::addColumn<QString>("componentName");
        QTest::addColumn<double>("radius");
        QTest::addColumn<QSize>("target");
        QTest::newRow("opening") << u"OpeningBackdrop"_s << 30.0 << QSize(260, 100);
        QTest::newRow("overview") << u"OverviewBackdrop"_s << 36.0 << QSize(268, 129);
        QTest::newRow("detail") << u"DetailTabsBackdrop"_s << 54.0 << QSize(308, 148);
    }
    void backdropGeometry()
    {
        QFETCH(QString, componentName);
        QFETCH(double, radius);
        QFETCH(QSize, target);
        QQmlEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine,
                                QUrl::fromLocalFile(QString::fromUtf8(FIXTURE_ROOT)
                                                    + u"/qml/Yalahoo/Music/components/"_s
                                                    + componentName + u".qml"_s));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QQuickItem source;
        QVariantMap properties{ { u"width"_s, 200.0 },
                                { u"height"_s, 40.0 },
                                { u"sceneSource"_s, QVariant::fromValue(&source) },
                                { u"sceneX"_s, 100.0 },
                                { u"sceneY"_s, 80.0 } };
        if (componentName == "OpeningBackdrop"_L1) {
            properties.insert(u"viewport"_s, QSize(1280, 720));
            properties.insert(u"pageColor"_s, QColor(Qt::white));
            properties.insert(u"surfaceColor"_s, QColor(Qt::gray));
            properties.insert(u"paperAlpha"_s, .5);
        } else if (componentName == "OverviewBackdrop"_L1)
            properties.insert(u"labelWidth"_s, 120.0);
        else {
            properties.insert(u"contentSource"_s, QVariant::fromValue(&source));
            properties.insert(u"contentY"_s, 20.0);
        }
        std::unique_ptr<QObject> root(component.createWithInitialProperties(properties));
        QVERIFY2(root, qPrintable(component.errorString()));
        auto *crop = root->property("sceneTexture").value<QObject *>();
        auto *intermediate = root->property("horizontalTexture").value<QObject *>();
        QVERIFY(crop);
        QVERIFY(intermediate);
        QCOMPARE(crop->property("sourceRect").toRectF(),
                 QRectF(100 - radius, 80 - radius, target.width(), target.height()));
        QCOMPARE(crop->property("textureSize").toSize(), target);
        QCOMPARE(intermediate->property("textureSize").toSize(), target);
        QCOMPARE(intermediate->property("sourceItem").value<QObject *>(),
                 root->property("horizontalPass").value<QObject *>());
        auto *horizontal = root->property("horizontalPass").value<QQuickItem *>();
        QVERIFY(horizontal);
        QCOMPARE(horizontal->size(), QSizeF(target));
        QCOMPARE(horizontal->property("sampleStep").value<QVector2D>(),
                 QVector2D(1.0f / target.width(), 0));
        QObject *overlay = nullptr;
        if (componentName == "DetailTabsBackdrop"_L1) {
            overlay = horizontal->property("overlay").value<QObject *>();
            QVERIFY(overlay);
            QCOMPARE(overlay->property("textureSize").toSize(), target);
        }
        QVERIFY(root->setProperty("sceneX", 110.0));
        QCOMPARE(crop->property("sourceRect").toRectF().x(), 110 - radius);
        if (componentName == "OverviewBackdrop"_L1) {
            root->setProperty("labelWidth", 130.0);
            QCOMPARE(crop->property("textureSize").toSize(), QSize(278, 129));
        } else {
            root->setProperty("width", 210.0);
            QCOMPARE(crop->property("textureSize").toSize(),
                     QSize(target.width() + 10, target.height()));
            if (overlay)
                QCOMPARE(overlay->property("textureSize").toSize(),
                         QSize(target.width() + 10, target.height()));
        }
        QVERIFY(warnings.isEmpty());
    }
};
int main(int argc, char **argv)
{
    verification::prepareApplication();
    QGuiApplication app(argc, argv);
    RenderSupportTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "RenderSupportTest.moc"
