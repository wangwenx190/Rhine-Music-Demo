// Creates the render-thread coordinator behind RhiItem's value synchronization boundary.
// Qt owns the returned renderer; it never borrows live GUI business objects.

#pragma once
#include <functional>
class QQuickRhiItemRenderer;

namespace yalahoo::rhi {
class RhiItem;
// Optional output preparation lets unit tests exercise initialization failure before first sync.
QQuickRhiItemRenderer *createSceneRenderer(RhiItem *receiver,
                                           std::function<void()> outputPreparation = { });
}
