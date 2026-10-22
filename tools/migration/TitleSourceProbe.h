// Runs the title component probe against production objects. Anonymous inputs and explicit
// completion keep observations separate from expected source values.

#pragma once
#include <QString>
class QGuiApplication;
int probeTitleSource(QGuiApplication &app, const QString &output, const QString &source);
