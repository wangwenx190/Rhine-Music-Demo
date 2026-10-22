// Creates the render-thread coordinator behind RhiItem's value synchronization boundary.
// Qt owns the returned renderer; it never borrows live GUI business objects.

#pragma once
class QQuickRhiItemRenderer;

namespace yalahoo::rhi {
QQuickRhiItemRenderer *createSceneRenderer();
}
