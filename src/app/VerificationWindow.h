#pragma once
#include <QWindow>
#include <QJsonObject>

namespace yalahoo::verification {
inline Qt::WindowFlags windowFlags(bool frameless = false)
{
    auto flags = Qt::Window | Qt::WindowStaysOnBottomHint | Qt::WindowDoesNotAcceptFocus;
    if (frameless)
        flags |= Qt::FramelessWindowHint;
    return flags;
}
inline void prepareWindow(QWindow &window, bool frameless = false)
{
    window.setFlags(windowFlags(frameless));
}
inline bool windowPolicySatisfied(const QWindow &window)
{
    return window.flags().testFlag(Qt::WindowStaysOnBottomHint)
            && window.flags().testFlag(Qt::WindowDoesNotAcceptFocus) && !window.isActive();
}
inline QJsonObject windowPolicy(const QWindow &window)
{
    return { { "staysOnBottom", window.flags().testFlag(Qt::WindowStaysOnBottomHint) },
             { "doesNotAcceptFocus", window.flags().testFlag(Qt::WindowDoesNotAcceptFocus) },
             { "active", window.isActive() },
             { "passed", windowPolicySatisfied(window) } };
}
}
