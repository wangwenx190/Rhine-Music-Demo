// Coordinates SDR/HDR output negotiation and transfer parameters. Output changes retain business
// state while surface changes follow explicit epochs.

#pragma once
#include <QObject>
#include <memory>

class QQuickWindow;
namespace yalahoo {
class DisplayMonitor;
}

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
    bool hdrActive() const;
    bool ready() const;
    float whiteScale() const;
    float headroom() const;
    bool hdr10() const;
Q_SIGNALS:
    void changed();

private:
    struct Private;
    std::unique_ptr<Private> d;
};
}
