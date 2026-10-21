#include "VerificationWindow.h"
#include "GraphicsBackend.h"
#include <QtTest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QQuickWindow>
#include "JsonFile.h"

namespace {
// Bounds for asynchronous completion and event delivery; fixture values remain independent.
constexpr int NativeEventSettleMilliseconds = 50;
}

using namespace Qt::StringLiterals;
#if defined(Q_OS_WIN)
#  include <windows.h>
#endif
class VerificationWindowTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(VerificationWindowTest)
public:
    VerificationWindowTest() = default;

private Q_SLOTS:
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

private:
    static auto showVerifiedWindow(QSignalSpy &swaps, QQuickWindow &window, auto &record,
                                   const QString &name)
    {
        const auto before = swaps.size();
        window.show();
        window.update();
        const bool visible = QTest::qWaitFor(
                [&] {
                    bool presented = swaps.size() > before && window.isExposed();
#if defined(Q_OS_MACOS)
                    presented = presented
                            && yalahoo::verification::nativeWindowPolicy(window)[u"onScreen"_s]
                                       .toBool();
#endif
                    return presented;
                },
                5000);
        record(name, visible);
    }
private Q_SLOTS:
    void visibleNativeWindow()
    {
        if (qEnvironmentVariableIntValue("YALAHOO_VERIFY_VISIBLE") != 1)
            QSKIP("Visible verification requires explicit opt-in");
        QQuickWindow window;
        window.setPersistentGraphics(false);
        window.setPersistentSceneGraph(false);
        window.resize(640, 360);
        yalahoo::verification::prepareWindow(window, true);
        QCOMPARE(window.requestedFormat().swapInterval(), 1);
        QSignalSpy swaps(&window, &QQuickWindow::frameSwapped);
        QJsonArray stages;
        bool passed = true;
        const auto record = [&](const QString &name, bool rendered = true) {
            const auto policy = yalahoo::verification::windowPolicy(window);
            const bool valid = rendered && policy[u"passed"_s].toBool();
            stages.append(QJsonObject{ { u"stage"_s, name },
                                       { u"passed"_s, valid },
                                       { u"frameSwaps"_s, swaps.size() },
                                       { u"windowPolicy"_s, policy } });
            passed = passed && valid;
        };
        const auto show = [&](const QString &name) {
            return showVerifiedWindow(swaps, window, record, name);
        };
        record(u"before-show"_s);
        show(u"first-show"_s);
        if (passed) {
            for (int cycle = 0; cycle < 10; ++cycle) {
                window.hide();
                QTest::qWait(NativeEventSettleMilliseconds);
                record(u"hide-%1"_s.arg(cycle), !window.isVisible());
                window.releaseResources();
                window.destroy();
                yalahoo::verification::prepareWindow(window, cycle % 2 == 0);
                window.resize(640 + cycle * 2, 360 + cycle * 2);
                record(u"recreate-%1"_s.arg(cycle), !window.isVisible());
                show(u"restore-%1"_s.arg(cycle));
                if (!passed)
                    break;
            }
            window.showMinimized();
            const bool minimized = QTest::qWaitFor(
                    [&] {
                        bool hidden =
                                window.windowState() == Qt::WindowMinimized && !window.isExposed();
#if defined(Q_OS_MACOS)
                        hidden = hidden
                                && yalahoo::verification::nativeWindowPolicy(
                                           window)[u"miniaturized"_s]
                                           .toBool();
#endif
                        return hidden;
                    },
                    5000);
            record(u"minimize"_s, minimized);
            window.showNormal();
            show(u"show-normal"_s);
        }
        window.hide();
        QTest::qWait(NativeEventSettleMilliseconds);
        record(u"final-hide"_s, !window.isVisible());
        const QJsonObject report{
            { u"passed"_s, passed },
            { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
            { u"graphicsApi"_s, int(window.rendererInterface()->graphicsApi()) },
            { u"devicePixelRatio"_s, window.devicePixelRatio() },
            { u"requestedSwapInterval"_s, window.requestedFormat().swapInterval() },
            { u"stages"_s, stages },
            { u"desktopCaptured"_s, false }
        };
        QVERIFY(yalahoo::writeJsonFile(QString::fromUtf8(VISIBLE_WINDOW_REPORT),
                                       QJsonDocument(report)));
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
        QVERIFY(native[u"nativeWindowAvailable"_s].toBool());
        QVERIFY(native[u"belowNormal"_s].toBool());
        QVERIFY(native[u"activationProhibited"_s].toBool());
        QVERIFY(native[u"ignoresMouseEvents"_s].toBool());
        QVERIFY(!native[u"canBecomeKey"_s].toBool());
        QVERIFY(!native[u"canBecomeMain"_s].toBool());
        QVERIFY(native[u"foregroundOwnershipPassed"_s].toBool());
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
