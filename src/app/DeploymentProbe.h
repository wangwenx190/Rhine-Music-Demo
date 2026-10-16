#pragma once
#include <QString>
#include <QStringList>
class QGuiApplication;
class QQuickWindow;
namespace yalahoo {
int verifyDeployment(QGuiApplication &app, QQuickWindow &window, const QString &reportPath,
                     const QStringList &qmlWarnings);
}
