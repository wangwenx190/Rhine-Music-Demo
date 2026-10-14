#pragma once
#include <QWindow>
#include <QJsonObject>

namespace yalahoo::verification {
inline void prepareApplication()
{
#if defined(Q_OS_MACOS)
    qputenv("QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM", "1");
#endif
}
#if defined(Q_OS_MACOS)
void prepareNativeWindow(QWindow &window);
QJsonObject nativeWindowPolicy(const QWindow &window);
#endif
inline Qt::WindowFlags windowFlags(bool frameless = false)
{
    auto flags = Qt::Window | Qt::WindowStaysOnBottomHint | Qt::WindowDoesNotAcceptFocus;
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
    const bool flags = window.flags().testFlag(Qt::WindowStaysOnBottomHint)
            && window.flags().testFlag(Qt::WindowDoesNotAcceptFocus) && !window.isActive();
#if defined(Q_OS_MACOS)
    return flags && nativeWindowPolicy(window)["passed"].toBool();
#else
    return flags;
#endif
}
inline QJsonObject windowPolicy(const QWindow &window)
{
    QJsonObject result{ { "staysOnBottom", window.flags().testFlag(Qt::WindowStaysOnBottomHint) },
                        { "doesNotAcceptFocus",
                          window.flags().testFlag(Qt::WindowDoesNotAcceptFocus) },
                        { "active", window.isActive() },
                        { "passed", windowPolicySatisfied(window) } };
#if defined(Q_OS_MACOS)
    result.insert("native", nativeWindowPolicy(window));
#endif
    return result;
}
}
