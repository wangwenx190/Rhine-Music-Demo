// Builds review inputs for the production material equations. The probe records actual uploads
// and output independently of mathematical expectations.

#pragma once
#include <QString>
class QGuiApplication;
namespace yalahoo {
class SceneAdapter;
int reviewMaterials(QGuiApplication &, SceneAdapter &, const QString &, const QString &);
}
