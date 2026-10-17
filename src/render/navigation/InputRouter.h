#pragma once
#include "SelectionController.h"
#include <QElapsedTimer>
#include <QQuickWindow>
#include <QWheelEvent>

namespace yalahoo {
class InputRouter final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(int rowDirection READ rowDirection NOTIFY directionChanged)
    Q_PROPERTY(int columnDirection READ columnDirection NOTIFY directionChanged)
    Q_PROPERTY(int navigationDirection READ navigationDirection NOTIFY directionChanged)
public:
    explicit InputRouter(SelectionController *selection, QObject *parent = nullptr);
    void attach(QQuickWindow *window);
    bool enabled() const;
    void setEnabled(bool enabled);
    void reset();
    int consume(double delta, double milliseconds);
    bool wheel(QWheelEvent *event, QQuickWindow *window);
    bool key(QKeyEvent *event, QQuickWindow *window);
    bool swipe(QPointF delta, double milliseconds);
    Q_INVOKABLE bool ownsFocus() const;
    Q_INVOKABLE bool row(int direction);
    Q_INVOKABLE bool lane(int direction);
    bool selectRow(double row);
    void flush();
    double remainder() const;
    int rowDirection() const;
    int columnDirection() const;
    int navigationDirection() const;
Q_SIGNALS:
    void enabledChanged();
    void directionChanged();
    void inputReceived();
    void navigationCommitted(quint64 revision);
    void overviewToggleRequested();
    void overviewReturnRequested();

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    bool commit(int direction, bool column);
    bool queue(ArchiveCell target);
    void setNavigationDirection(int direction, bool column);
    QPointer<SelectionController> m_selection;
    QPointer<QQuickWindow> m_window;
    QElapsedTimer m_timer;
    double m_remainder = 0, m_lastTime = -1e20;
    int m_direction = 0, m_context = -1;
    int m_rowDirection = 0, m_columnDirection = 0, m_navigationDirection = 0;
    bool m_enabled = true;
    bool m_committing = false;
    std::optional<ArchiveCell> m_pending;
};
}
