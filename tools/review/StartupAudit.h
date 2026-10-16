#pragma once
#include <QString>
#include <QJsonObject>
class QGuiApplication;
int reviewStartup(QGuiApplication &app, const QString &output, double seconds,
                  const QJsonObject &bootstrap, const QString &scenario = QStringLiteral("matrix"));
