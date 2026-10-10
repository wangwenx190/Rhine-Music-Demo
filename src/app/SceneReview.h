#pragma once
#include <QString>
class QGuiApplication;
class QQuickWindow;
namespace yalahoo {
class SceneAdapter;
int reviewScene(QGuiApplication &app, QQuickWindow &window, SceneAdapter &scene, const QString &fixture, const QString &report);
}
