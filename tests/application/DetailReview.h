// Drives deterministic detail transitions through the production controllers. Captures retain
// request identity so interrupted transitions cannot publish stale evidence.

#pragma once
#include <QString>
class QGuiApplication;
class QQuickWindow;
namespace yalahoo {
class SceneAdapter;
class LibrarySession;
class PresentationController;
class MotionController;
int reviewDetail(QGuiApplication &app, QQuickWindow &window, SceneAdapter &scene,
                 LibrarySession &library, PresentationController &presentation,
                 MotionController &motion, const QString &output);
}
