#include "FrameClock.h"
#include "PanelController.h"

using namespace Qt::StringLiterals;

namespace yalahoo {
PanelController::PanelController(FrameClock *clock, QObject *parent) : QObject(parent)
{
    connect(clock, &FrameClock::frame, this, [this](double, double dt) {
        m_surface.advance(dt);
    });
}
void PanelController::open(const QString &panel)
{
    if (panel != "search"_L1 && panel != "library"_L1 && panel != "playback"_L1
        && panel != "settings"_L1)
        return;
    m_panel = panel;
    Q_EMIT changed();
    m_surface.show(m_reduced);
}
void PanelController::close()
{
    m_surface.hide(m_reduced, [this] {
        m_panel.clear();
        Q_EMIT changed();
        Q_EMIT closed();
    });
}
void PanelController::reset()
{
    close();
    m_surface.finish();
}
void PanelController::setReducedMotion(bool reduced)
{
    m_reduced = reduced;
    if (reduced)
        m_surface.finish();
}
}
