// Coalesces native output topology notifications before a controller reconciles its route.
// The controller owns devices/timer and supplies the connection context for its callback.

#pragma once
#include <QMediaDevices>
#include <QTimer>
#include <utility>

namespace yalahoo {
template <typename Refresh>
void connectAudioOutputChanges(QMediaDevices &devices, QTimer &timer, QObject *context,
                               Refresh &&refresh)
{
    timer.setSingleShot(true);
    QObject::connect(&devices, &QMediaDevices::audioOutputsChanged, &timer,
                     qOverload<>(&QTimer::start));
    QObject::connect(&timer, &QTimer::timeout, context, std::forward<Refresh>(refresh));
}
}
