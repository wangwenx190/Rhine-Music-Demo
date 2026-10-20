// Replays cross-component product interactions with anonymous library data. Observes real
// production state rather than maintaining a separate UI simulation.

#pragma once
#include <QString>
class QGuiApplication;
class QQuickWindow;
namespace yalahoo {
class SceneAdapter;
int reviewIntegration(QGuiApplication &app, QQuickWindow &window, SceneAdapter &scene,
                      const QString &output, double seconds);
}
