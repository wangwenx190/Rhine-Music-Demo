// Runs the overview component probe against production objects. Anonymous inputs and explicit
// completion keep observations separate from expected source values.

#pragma once
#include <QString>
class QGuiApplication;
int reviewOverviewVisual(QGuiApplication &app, const QString &output);
