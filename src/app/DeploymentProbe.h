// Inspects the deployed runtime and its loaded Qt resources. Reports concrete plugin, path and
// module facts from the running package.

#pragma once
#include <QString>
#include <QStringList>
class QGuiApplication;
class QQuickWindow;
namespace yalahoo {
int verifyDeployment(QGuiApplication &app, QQuickWindow &window, const QString &reportPath,
                     const QStringList &qmlWarnings);
}
