#include "VerificationWindow.h"
#include "GraphicsBackend.h"
#include <QtTest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QQuickWindow>
#include <QSaveFile>
#if defined(Q_OS_WIN)
#  include <windows.h>
#endif
class VerificationWindowTest : public QObject
{
    Q_OBJECT
private slots:
    void foregroundOwnership()
    {
        using yalahoo::verification::foregroundOwnershipSafe;
        QVERIFY(foregroundOwnershipSafe(10, 10, 42, false, false));
        QVERIFY(foregroundOwnershipSafe(10, 11, 42, false, false));
        QVERIFY(!foregroundOwnershipSafe(42, 11, 42, false, false));
        QVERIFY(!foregroundOwnershipSafe(10, 42, 42, false, false));
        QVERIFY(!foregroundOwnershipSafe(10, 11, 42, true, false));
        QVERIFY(!foregroundOwnershipSafe(10, 10, 42, false, true));
        QVERIFY(!foregroundOwnershipSafe(0, 11, 42, false, false));
        QVERIFY(!foregroundOwnershipSafe(10, 0, 42, false, false));
    }
    void visibleNativeWindow()
    {
        if (qEnvironmentVariableIntValue("YALAHOO_VERIFY_VISIBLE") != 1)
            QSKIP("Visible verification requires explicit opt-in");
        QQuickWindow window;
        window.setPersistentGraphics(false);
        window.setPersistentSceneGraph(false);
        window.resize(640, 360);
        yalahoo::verification::prepareWindow(window, true);
        QSignalSpy swaps(&window, &QQuickWindow::frameSwapped);
        QJsonArray stages;
        bool passed = true;
        const auto record = [&](const QString &name, bool rendered = true) {
            const auto policy = yalahoo::verification::windowPolicy(window);
            const bool valid = rendered && policy["passed"].toBool();
            stages.append(QJsonObject{ { "stage", name },
                                       { "passed", valid },
                                       { "frameSwaps", swaps.size() },
                                       { "windowPolicy", policy } });
            passed = passed && valid;
        };
        const auto show = [&](const QString &name) {
            const auto before = swaps.size();
            window.show();
            window.update();
            if (swaps.size() == before)
                swaps.wait(3000);
            QTest::qWait(100);
            bool visible = swaps.size() > before && window.isExposed();
#if defined(Q_OS_MACOS)
            visible = visible
                    && yalahoo::verification::nativeWindowPolicy(window)["onScreen"].toBool();
#endif
            record(name, visible);
        };
        record("before-show");
        show("first-show");
        if (passed) {
            for (int cycle = 0; cycle < 10; ++cycle) {
                window.hide();
                QTest::qWait(50);
                record(QStringLiteral("hide-%1").arg(cycle), !window.isVisible());
                window.releaseResources();
                window.destroy();
                yalahoo::verification::prepareWindow(window, cycle % 2 == 0);
                window.resize(640 + cycle * 2, 360 + cycle * 2);
                record(QStringLiteral("recreate-%1").arg(cycle), !window.isVisible());
                show(QStringLiteral("restore-%1").arg(cycle));
                if (!passed)
                    break;
            }
            window.showMinimized();
            QTest::qWait(100);
            record("minimize", window.windowState() == Qt::WindowMinimized);
            window.showNormal();
            show("show-normal");
        }
        window.hide();
        QTest::qWait(50);
        record("final-hide", !window.isVisible());
        QSaveFile output(VISIBLE_WINDOW_REPORT);
        QVERIFY(output.open(QIODevice::WriteOnly));
        const QJsonObject report{ { "passed", passed },
                                  { "qtVersion", qVersion() },
                                  { "graphicsApi", int(window.rendererInterface()->graphicsApi()) },
                                  { "devicePixelRatio", window.devicePixelRatio() },
                                  { "stages", stages },
                                  { "desktopCaptured", false } };
        QVERIFY(output.write(QJsonDocument(report).toJson()) > 0);
        QVERIFY(output.commit());
        QVERIFY2(passed,
                 "Visible window policy/exposure/present failed; see native-window-visible.json");
    }
    void hiddenNativeWindow()
    {
        QWindow window;
        yalahoo::verification::prepareWindow(window, true);
        QVERIFY(!window.isVisible());
        window.create();
        QVERIFY(!window.isVisible());
        QVERIFY(yalahoo::verification::windowPolicySatisfied(window));
        QVERIFY(window.flags().testFlag(Qt::FramelessWindowHint));
#if defined(Q_OS_MACOS)
        const auto native = yalahoo::verification::nativeWindowPolicy(window);
        QVERIFY(native["nativeWindowAvailable"].toBool());
        QVERIFY(native["belowNormal"].toBool());
        QVERIFY(native["activationProhibited"].toBool());
        QVERIFY(native["ignoresMouseEvents"].toBool());
        QVERIFY(!native["canBecomeKey"].toBool());
        QVERIFY(!native["canBecomeMain"].toBool());
        QVERIFY(native["foregroundOwnershipPassed"].toBool());
#endif
#if defined(Q_OS_WIN)
        const auto handle = reinterpret_cast<HWND>(window.winId());
        const auto style = GetWindowLongPtrW(handle, GWL_EXSTYLE);
        QVERIFY((style & WS_EX_NOACTIVATE) != 0);
        QVERIFY((style & WS_EX_TOPMOST) == 0);
        QVERIFY(GetForegroundWindow() != handle);
#endif
        // Flag changes/recreation remain hidden and non-activating too.
        yalahoo::verification::prepareWindow(window);
        QVERIFY(!window.isVisible());
        QVERIFY(yalahoo::verification::windowPolicySatisfied(window));
        window.destroy();
        window.create();
        QVERIFY(!window.isVisible());
        QVERIFY(yalahoo::verification::windowPolicySatisfied(window));
    }
};
int main(int argc, char **argv)
{
    yalahoo::verification::prepareApplication();
    if (!yalahoo::selectGraphicsBackend())
        return 2;
    QGuiApplication app(argc, argv);
    VerificationWindowTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "VerificationWindowTest.moc"
