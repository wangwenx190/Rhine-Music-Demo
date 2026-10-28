// Exercises overview framing, label admission and return navigation. Captures share production
// state and preserve the verification window lifecycle.

#pragma once
#include <QString>
class QGuiApplication;
class QQuickWindow;
namespace yalahoo {
class SceneAdapter;
class OverviewController;
class PresentationController;
class LibrarySession;
class MotionController;
int reviewOverview(QGuiApplication &app, QQuickWindow &window, SceneAdapter &scene,
                   OverviewController &overview, PresentationController &presentation,
                   LibrarySession &library, MotionController &motion, const QString &output);
}
