// Provides visual-tree lookup, stage dispatch and common bounds for isolated verification.
// Visual children, rather than QObject ownership, determine the actual rendered item tree.

#pragma once
#include <QQuickItem>
#include <QString>
#include <algorithm>
#include <cstddef>
#include <span>

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
template <typename Context>
struct StageRoute final
{
    int stage;
    void (*run)(Context &);
};
template <typename Context, std::size_t Count>
bool dispatchStage(const StageRoute<Context> (&routes)[Count], int stage, Context &context)
{
    const auto entries = std::span(routes);
    const auto route = std::find_if(entries.begin(), entries.end(), [stage](const auto &entry) {
        return entry.stage == stage;
    });
    if (route == entries.end())
        return false;
    route->run(context);
    return true;
}
inline constexpr auto DefaultDeadlineMilliseconds = 60000;
inline constexpr auto WorkerDrainDeadlineMilliseconds = 30000;
inline constexpr auto WidthPixels = 1280;
inline constexpr auto HeightPixels = 720;
inline constexpr auto LifecycleRebuildCount = 10;
inline constexpr auto MinimumForegroundPixels = 100;
}
