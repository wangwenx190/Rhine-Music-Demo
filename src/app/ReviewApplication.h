// Runs graphical verification with the shared pre-application window/backend policy.
// The callback finishes while its QGuiApplication and platform backend are still alive.

#pragma once
#include "GraphicsBackend.h"
#include "VerificationWindow.h"
#include <QGuiApplication>
#include <utility>

namespace yalahoo::review {
template <typename Callback>
int runApplication(int &argc, char **argv, Callback &&callback)
{
    verification::prepareApplication();
    if (!selectGraphicsBackend())
        return 2;
    QGuiApplication app(argc, argv);
    if (!probeOptionalGraphicsBackend())
        return 22;
    return std::forward<Callback>(callback)(app);
}
}
