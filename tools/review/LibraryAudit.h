#pragma once
#include <QString>
class QGuiApplication;
int reviewLibrary(QGuiApplication &app, const QString &output, bool playbackVerification = false,
                  bool settingsVerification = false);
