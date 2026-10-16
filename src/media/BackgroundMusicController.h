#pragma once
#include <QAudioOutput>
#include <QAudioDevice>
#include <QMediaPlayer>
#include <QMediaDevices>
#include <QElapsedTimer>
#include <QTimer>

namespace yalahoo {
class AudioAmbienceTest;
class BackgroundMusicController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY settingsChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY settingsChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
public:
    enum class SongActivity {
        Stopped,
        Paused,
        Playing
    };
    explicit BackgroundMusicController(QObject *parent = nullptr);
    ~BackgroundMusicController() override;
    bool enabled() const
    {
        return m_enabled;
    }
    double volume() const
    {
        return m_volume;
    }
    bool playing() const
    {
        return m_player.isPlaying();
    }
    QString error() const
    {
        return m_error;
    }
    bool silent() const
    {
        return m_output.volume() == 0;
    }
    void setEnabled(bool enabled);
    void setVolume(double volume);
    void setSongActivity(SongActivity activity);
    void start();
    void stop();
signals:
    void settingsChanged();
    void stateChanged();
    void silenceReached();

private:
    friend class AudioAmbienceTest;
    void reconcile();
    void advanceFade();
    void refreshOutput();
    QMediaDevices m_devices;
    QAudioOutput m_output;
    QMediaPlayer m_player;
    QTimer m_fade;
    QElapsedTimer m_elapsed;
    QString m_error;
    SongActivity m_song = SongActivity::Stopped;
    double m_volume = .18, m_from = 0, m_target = 0;
    int m_duration = 0;
    bool m_enabled = true, m_active = false, m_available = false, m_refreshPending = false;
};
}
