// Plays bundled UI feedback through an independent Qt Multimedia route. Trigger policy avoids
// synthesizing delayed sounds for events that occurred while muted.

#pragma once
#include <QObject>
#include <QString>
#include <memory>

namespace yalahoo {
namespace feedbackAudio {
// Bounded simultaneous feedback voices and normalized default volume.
inline constexpr auto VoiceCount = 4;
}

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
    bool enabled() const;
    double volume() const;
    bool available() const;
    void setEnabled(bool enabled);
    void setVolume(double volume);
    Q_INVOKABLE bool trigger(const QString &event);
    Q_INVOKABLE void preview();
    void stop();
Q_SIGNALS:
    void settingsChanged();

private:
    friend class AudioAmbienceTest;
    struct Private;
    std::unique_ptr<Private> d;
};
}
