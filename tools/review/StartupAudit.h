#pragma once
#include <QString>
using Qt::StringLiterals::operator""_s;
#include <QString>
#include <QJsonObject>
class QGuiApplication;
int reviewStartup(QGuiApplication &app, const QString &output, double seconds,
                  const QJsonObject &bootstrap, const QString &scenario = u"matrix"_s);

int reviewGui(const QString &output, bool narrowOnly = false);
