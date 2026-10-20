#pragma once
#include <QObject>
#include <QPointer>
#include <QQuickWindow>

namespace yalahoo {
class SceneGraphLifecycle : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SceneGraphLifecycle)
    Q_PROPERTY(QQuickWindow *window READ window WRITE setWindow NOTIFY windowChanged FINAL)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged FINAL)
public:
    SceneGraphLifecycle() = default;
    using QObject::QObject;
    QQuickWindow *window() const;
    void setWindow(QQuickWindow *window);
    bool available() const;
Q_SIGNALS:
    void windowChanged();
    void availableChanged();

private:
    void setAvailable(bool available);
    QPointer<QQuickWindow> m_window;
    QMetaObject::Connection m_initialized, m_invalidated, m_destroyed;
    quint64 m_epoch = 0;
    bool m_available = false;
};
}
