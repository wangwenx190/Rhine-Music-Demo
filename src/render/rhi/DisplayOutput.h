// Coordinates SDR/HDR output negotiation and transfer parameters. Output changes retain business
// state while surface changes follow explicit epochs.

#pragma once
#include "DisplayState.h"
#include <QPointer>
#include <QTimer>
#include <QQuickWindow>
#include <atomic>

namespace yalahoo {
class DisplayOutput : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DisplayOutput)
    Q_PROPERTY(bool hdrActive READ hdrActive NOTIFY changed FINAL)
    Q_PROPERTY(bool ready READ ready NOTIFY changed FINAL)
    Q_PROPERTY(float whiteScale READ whiteScale NOTIFY changed FINAL)
    Q_PROPERTY(float headroom READ headroom NOTIFY changed FINAL)
    Q_PROPERTY(bool hdr10 READ hdr10 NOTIFY changed FINAL)
public:
    explicit DisplayOutput(QObject *parent = nullptr);
    explicit DisplayOutput(std::unique_ptr<DisplayMonitor> monitor, QObject *parent = nullptr);
    ~DisplayOutput() override;
    void attach(QQuickWindow &window);
    bool hdrActive() const
    {
        return m_active;
    }
    bool ready() const
    {
        return m_ready;
    }
    float whiteScale() const
    {
        return m_active && !m_useLinearFallback ? m_state.whiteScale : 1;
    }
    float headroom() const
    {
        return m_active ? m_state.headroom : 1;
    }
    bool hdr10() const
    {
        return m_active && m_requestedHdr10;
    }
Q_SIGNALS:
    void changed();

private:
    void refresh();
    void configure(bool hdr);
    void recoverOutput();
    QPointer<QQuickWindow> m_window;
    std::unique_ptr<DisplayMonitor> m_monitor;
    QTimer m_poll;
    DisplayState m_state;
    bool m_requested = false;
    bool m_requestedHdr10 = false;
    bool m_active = false;
    bool m_ready = true;
    bool m_reconfiguring = false;
    bool m_rejected = false;
    bool m_useLinearFallback = false;
    std::atomic<quint64> m_epoch = 0;
    quint64 m_reportedEpoch = quint64(-1); // Render-thread state only.
};
}
