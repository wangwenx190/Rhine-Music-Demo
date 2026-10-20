#pragma once
#include <QString>
using Qt::StringLiterals::operator""_s;
#include <QJsonObject>
class QGuiApplication;
int reviewStartup(QGuiApplication &app, const QString &output, double seconds,
                  const QJsonObject &bootstrap, const QString &scenario = u"matrix"_s,
                  bool observeOnly = false);

int reviewGui(const QString &output, bool narrowOnly = false, bool representative = false);

int reviewWindowLifecycle(const QString &output);
