#pragma once
#include <QWindow>
#include <QJsonObject>

namespace yalahoo::verification {
inline bool foregroundVerification()
{
    return qEnvironmentVariableIntValue("YALAHOO_VERIFY_FOREGROUND") == 1;
}
#if defined(Q_OS_MACOS)
void prepareNativeApplication();
#endif
inline void prepareApplication()
{
#if defined(Q_OS_MACOS)
    qputenv("QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM", "1");
    prepareNativeApplication();
#endif
}
#if defined(Q_OS_MACOS)
void prepareNativeWindow(QWindow &window);
QJsonObject nativeWindowPolicy(const QWindow &window);
#endif
inline Qt::WindowFlags windowFlags(bool frameless = false)
{
    Qt::WindowFlags flags = Qt::Window;
    if (!foregroundVerification())
        flags |= Qt::WindowStaysOnBottomHint | Qt::WindowDoesNotAcceptFocus;
    if (frameless)
        flags |= Qt::FramelessWindowHint;
    return flags;
}
inline void prepareWindow(QWindow &window, bool frameless = false)
{
    prepareApplication();
    window.setFlags(windowFlags(frameless));
#if defined(Q_OS_MACOS)
    prepareNativeWindow(window);
#endif
}
inline bool windowPolicySatisfied(const QWindow &window)
{
    const bool flags = foregroundVerification()
            ? !window.flags().testFlag(Qt::WindowStaysOnBottomHint)
                    && !window.flags().testFlag(Qt::WindowDoesNotAcceptFocus)
            : window.flags().testFlag(Qt::WindowStaysOnBottomHint)
                    && window.flags().testFlag(Qt::WindowDoesNotAcceptFocus) && !window.isActive();
#if defined(Q_OS_MACOS)
    return flags && nativeWindowPolicy(window)["passed"].toBool();
#else
    return flags;
#endif
}
inline QJsonObject windowPolicy(const QWindow &window)
{
    QJsonObject result{
        { "staysOnBottom", window.flags().testFlag(Qt::WindowStaysOnBottomHint) },
        { "mode",
          foregroundVerification() ? "user-authorized-foreground" : "nonactivating-bottom" },
        { "nonInterferenceEnforced", !foregroundVerification() },
        { "doesNotAcceptFocus", window.flags().testFlag(Qt::WindowDoesNotAcceptFocus) },
        { "active", window.isActive() },
        { "visible", window.isVisible() },
        { "exposed", window.isExposed() },
        { "passed", windowPolicySatisfied(window) }
    };
#if defined(Q_OS_MACOS)
    result.insert("native", nativeWindowPolicy(window));
#endif
    return result;
}
}
