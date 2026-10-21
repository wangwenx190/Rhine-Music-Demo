// Provides visual-tree lookup and common bounds for isolated verification code.
// Visual children, rather than QObject ownership, determine the actual rendered item tree.

#pragma once
#include <QQuickItem>
#include <QString>

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
inline constexpr auto DefaultDeadlineMilliseconds = 60000;
inline constexpr auto WorkerDrainDeadlineMilliseconds = 30000;
inline constexpr auto WidthPixels = 1280;
inline constexpr auto HeightPixels = 720;
inline constexpr auto LifecycleRebuildCount = 10;
inline constexpr auto MinimumForegroundPixels = 100;
}
