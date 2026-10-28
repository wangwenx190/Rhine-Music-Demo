// Applies the selected verification window policy before application creation and first show.
// Native guards preserve that policy when Qt replaces surfaces or window flags.

#pragma once
#include <QString>
using Qt::StringLiterals::operator""_s;
using Qt::StringLiterals::operator""_ba;
#include <QWindow>
#include <QJsonObject>

namespace yalahoo::verification {
// External app switches do not establish interference by this verification process.
// Both snapshots and activation/focus history must exclude our own process.
inline bool foregroundOwnershipSafe(qint64 initial, qint64 current, qint64 verificationPid,
                                    bool activation, bool focus)
{
    return initial > 0 && current > 0 && verificationPid > 0 && initial != verificationPid
            && current != verificationPid && !activation && !focus;
}
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
    qputenv("QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM", "1"_ba);
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
    return flags && nativeWindowPolicy(window)[u"passed"_s].toBool();
#else
    return flags;
#endif
}
inline QJsonObject windowPolicy(const QWindow &window)
{
    QJsonObject result{
        { u"staysOnBottom"_s, window.flags().testFlag(Qt::WindowStaysOnBottomHint) },
        { u"mode"_s,
          foregroundVerification() ? u"user-authorized-foreground"_s : u"nonactivating-bottom"_s },
        { u"nonInterferenceEnforced"_s, !foregroundVerification() },
        { u"doesNotAcceptFocus"_s, window.flags().testFlag(Qt::WindowDoesNotAcceptFocus) },
        { u"active"_s, window.isActive() },
        { u"visible"_s, window.isVisible() },
        { u"exposed"_s, window.isExposed() },
        { u"passed"_s, windowPolicySatisfied(window) }
    };
#if defined(Q_OS_MACOS)
    result.insert(u"native"_s, nativeWindowPolicy(window));
#endif
    return result;
}
}
