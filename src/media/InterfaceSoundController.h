#pragma once
#include <QMediaDevices>
#include <QAudioDevice>
#include <QSoundEffect>
#include <QHash>
#include <array>
#include <memory>

namespace yalahoo {
class AudioAmbienceTest;
class InterfaceSoundController : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(InterfaceSoundController)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY settingsChanged FINAL)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY settingsChanged FINAL)
    Q_PROPERTY(bool available READ available CONSTANT FINAL)
public:
    explicit InterfaceSoundController(QObject *parent = nullptr);
    ~InterfaceSoundController() override;
    bool enabled() const
    {
        return m_enabled;
    }
    double volume() const
    {
        return m_volume;
    }
    bool available() const
    {
        return !m_sources.isEmpty();
    }
    void setEnabled(bool enabled);
    void setVolume(double volume);
    Q_INVOKABLE bool trigger(const QString &event);
    Q_INVOKABLE void preview();
    void stop();
Q_SIGNALS:
    void settingsChanged();

private:
    friend class AudioAmbienceTest;
    void refreshOutput();
    QMediaDevices m_devices;
    QHash<QString, QUrl> m_sources;
    std::array<std::unique_ptr<QSoundEffect>, 4> m_pool;
    double m_volume = .22;
    bool m_enabled = true, m_refreshPending = false;
};
}
