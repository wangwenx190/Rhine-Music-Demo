// Runs the library component probe against production objects. Anonymous inputs and explicit
// completion keep observations separate from expected source values.

#pragma once
#include <QString>
class QGuiApplication;
int reviewLibrary(QGuiApplication &app, const QString &output, bool playbackVerification = false,
                  bool settingsVerification = false, bool representative = false);
