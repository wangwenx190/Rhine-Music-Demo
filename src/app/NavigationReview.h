#pragma once
#include <QString>
class QGuiApplication;
class QQuickWindow;
namespace yalahoo {
class SceneAdapter;
class LibrarySession;
class InputRouter;
class RulerController;
int reviewNavigation(QGuiApplication &app, QQuickWindow &window, SceneAdapter &scene,
                     LibrarySession &library, InputRouter &input, RulerController &ruler,
                     const QString &output);
}
