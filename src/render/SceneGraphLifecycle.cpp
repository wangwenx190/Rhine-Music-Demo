#include <QQuickWindow>
#include "SceneGraphLifecycle.h"

namespace yalahoo {
QQuickWindow *SceneGraphLifecycle::window() const
{
    return m_window;
}
bool SceneGraphLifecycle::available() const
{
    return m_available;
}
void SceneGraphLifecycle::setAvailable(bool available)
{
    if (m_available == available)
        return;
    m_available = available;
    Q_EMIT availableChanged();
}
void SceneGraphLifecycle::setWindow(QQuickWindow *window)
{
    if (m_window == window)
        return;
    disconnect(m_initialized);
    disconnect(m_invalidated);
    disconnect(m_destroyed);
    const auto epoch = ++m_epoch;
    m_window = window;
    if (window) {
        m_initialized = connect(
                window, &QQuickWindow::sceneGraphInitialized, this,
                [this, epoch] {
                    if (epoch == m_epoch)
                        setAvailable(true);
                },
                Qt::QueuedConnection);
        m_invalidated = connect(
                window, &QQuickWindow::sceneGraphInvalidated, this,
                [this, epoch] {
                    if (epoch == m_epoch)
                        setAvailable(false);
                },
                Qt::QueuedConnection);
        m_destroyed = connect(window, &QObject::destroyed, this, [this, epoch] {
            if (epoch != m_epoch)
                return;
            ++m_epoch;
            m_window = nullptr;
            setAvailable(false);
            Q_EMIT windowChanged();
        });
    }
    setAvailable(window != nullptr);
    Q_EMIT windowChanged();
}
}
