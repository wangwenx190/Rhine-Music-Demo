// Owns the independent Qt Multimedia background-music route. Visibility and user preferences
// control its intent without changing song playback.

#pragma once
#include <QObject>
#include <QString>
#include <memory>

namespace yalahoo {

class AudioAmbienceTest;
class BackgroundMusicController : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(BackgroundMusicController)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY settingsChanged FINAL)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY settingsChanged FINAL)
    Q_PROPERTY(bool playing READ playing NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged FINAL)
public:
    enum class SongActivity {
        Stopped,
        Paused,
        Playing
    };
    explicit BackgroundMusicController(QObject *parent = nullptr);
    ~BackgroundMusicController() override;
    bool enabled() const;
    double volume() const;
    bool playing() const;
    QString error() const;
    bool silent() const;
    void setEnabled(bool enabled);
    void setVolume(double volume);
    void setSongActivity(SongActivity activity);
    void start();
    void stop();
Q_SIGNALS:
    void settingsChanged();
    void stateChanged();
    void silenceReached();

private:
    friend class AudioAmbienceTest;
    struct Private;
    std::unique_ptr<Private> d;
};
}
