// Rebinds a clock-driven object's guarded source and one receiver-owned frame connection.
// Receivers expose advance(seconds); replacing or clearing a clock disconnects the old source.

#pragma once
#include "FrameClock.h"
#include <QPointer>

namespace yalahoo {
template <typename Receiver>
bool bindFrameClock(Receiver *receiver, QPointer<FrameClock> &current,
                    QMetaObject::Connection &connection, FrameClock *next)
{
    if (current == next)
        return false;
    QObject::disconnect(connection);
    current = next;
    if (next)
        connection =
                QObject::connect(next, &FrameClock::frame, receiver, [receiver](double, double dt) {
                    receiver->advance(dt);
                });
    return true;
}
}
