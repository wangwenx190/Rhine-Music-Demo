#pragma once
#include <QString>
class QGuiApplication;
class QQuickWindow;
namespace yalahoo {
class SceneAdapter;
int reviewIntegration(QGuiApplication &app, QQuickWindow &window, SceneAdapter &scene,
                      const QString &output, double seconds);
}
