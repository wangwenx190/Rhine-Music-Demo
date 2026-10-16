#include "PanelController.h"
namespace yalahoo {
PanelController::PanelController(FrameClock *clock, QObject *parent) : QObject(parent)
{
    connect(clock, &FrameClock::frame, this, [this](double, double dt) {
        m_surface.advance(dt);
    });
}
void PanelController::open(const QString &panel)
{
    if (panel != "search" && panel != "library" && panel != "playback" && panel != "settings")
        return;
    m_panel = panel;
    emit changed();
    m_surface.show(m_reduced);
}
void PanelController::close()
{
    m_surface.hide(m_reduced, [this] {
        m_panel.clear();
        emit changed();
        emit closed();
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
