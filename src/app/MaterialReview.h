#pragma once
#include <QString>
class QGuiApplication;
class QQuickWindow;
namespace yalahoo {
class SceneAdapter;
int reviewMaterials(QGuiApplication &, QQuickWindow &, SceneAdapter &, const QString &,
                    const QString &);
}
