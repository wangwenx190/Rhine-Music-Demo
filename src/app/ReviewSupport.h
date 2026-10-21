// Provides visual-tree lookup and monotonic timestamps for isolated verification code.
// Visual children, rather than QObject ownership, determine the actual rendered item tree.

#pragma once
#include <QQuickItem>
#include <QString>
#include <chrono>

namespace yalahoo::review {
inline QQuickItem *findVisual(QQuickItem *root, const QString &name)
{
    if (!root)
        return nullptr;
    if (root->objectName() == name)
        return root;
    for (auto *child : root->childItems())
        if (auto *found = findVisual(child, name))
            return found;
    return nullptr;
}
inline qint64 nowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
}
inline constexpr auto DefaultDeadlineMilliseconds = 60000;
inline constexpr auto WorkerDrainDeadlineMilliseconds = 30000;
inline constexpr auto WidthPixels = 1280;
inline constexpr auto HeightPixels = 720;
inline constexpr auto LifecycleRebuildCount = 10;
inline constexpr auto MinimumForegroundPixels = 100;
}
