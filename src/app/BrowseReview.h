#pragma once
#include <QString>
class QGuiApplication;
class QQuickWindow;
namespace yalahoo {
class SceneAdapter;
class LibrarySession;
int reviewBrowse(QGuiApplication &app, QQuickWindow &window, SceneAdapter &scene,
                 LibrarySession &library, const QString &output);
}
