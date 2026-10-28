// Owns product composition. Hosts can observe lifecycle boundaries without replacing services.
#pragma once
#include "ApplicationOptions.h"
#include <QVariantMap>
#include <functional>
class QGuiApplication;
class QQmlApplicationEngine;
class QQuickWindow;
namespace yalahoo {
class SceneAdapter;
class OverviewController;
class PresentationController;
class LibrarySession;
class MotionController;
class PlaybackController;
class InputRouter;
class RulerController;
struct ApplicationContext final
{
    QGuiApplication &app;
    QQmlApplicationEngine &engine;
    SceneAdapter &scene;
    OverviewController &overview;
    PresentationController &presentation;
    LibrarySession &library;
    MotionController &motion;
    PlaybackController &playback;
    InputRouter &input;
    RulerController &ruler;
    QQuickWindow *window = nullptr;
};
struct ApplicationLaunch final
{
    bool persistent = true;
    bool automaticPresentation = true;
    bool bindLibrary = true;
    bool bindOpening = true;
    bool anonymousLibrary = false;
    QVariantMap initialProperties;
    std::function<void(QQmlApplicationEngine &)> observeEngine;
    std::function<std::optional<int>(ApplicationContext &)> beforeLoad;
    std::function<std::optional<int>(ApplicationContext &)> afterLoad;
};
int runApplication(int argc, char **argv, const ApplicationOptions &options,
                   const ApplicationLaunch &launch);
}
