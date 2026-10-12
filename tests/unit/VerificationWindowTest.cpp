#include "VerificationWindow.h"
#include <QtTest>
#if defined(Q_OS_WIN)
#  include <windows.h>
#endif
class VerificationWindowTest : public QObject
{
    Q_OBJECT
private slots:
    void hiddenNativeWindow()
    {
        QWindow window;
        yalahoo::verification::prepareWindow(window, true);
        QVERIFY(!window.isVisible());
        window.create();
        QVERIFY(!window.isVisible());
        QVERIFY(yalahoo::verification::windowPolicySatisfied(window));
        QVERIFY(window.flags().testFlag(Qt::FramelessWindowHint));
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
    }
};
QTEST_MAIN(VerificationWindowTest)
#include "VerificationWindowTest.moc"
