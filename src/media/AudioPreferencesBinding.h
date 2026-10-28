// Synchronizes the three independent audio routes with preferences. Applying an import never
// echoes partially restored settings back to disk. All borrowed controllers outlive this binding.
#pragma once
#include <QObject>

namespace yalahoo {
class PreferencesStore;
class PlaybackController;
class BackgroundMusicController;
class InterfaceSoundController;
class AudioPreferencesBinding final : public QObject
{
    Q_DISABLE_COPY_MOVE(AudioPreferencesBinding)
public:
    AudioPreferencesBinding(PreferencesStore &preferences, PlaybackController &playback,
                            BackgroundMusicController &background, InterfaceSoundController &sounds,
                            QObject *parent = nullptr);

private:
    void apply();
    void persist();
    PreferencesStore &m_preferences;
    PlaybackController &m_playback;
    BackgroundMusicController &m_background;
    InterfaceSoundController &m_sounds;
    bool m_applying = false;
};
}
