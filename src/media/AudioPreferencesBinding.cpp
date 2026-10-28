#include "AudioPreferencesBinding.h"
#include "PreferencesStore.h"
#include "PlaybackController.h"
#include "BackgroundMusicController.h"
#include "InterfaceSoundController.h"
#include "ProductDefaults.h"
#include <QScopedValueRollback>

using namespace Qt::StringLiterals;
namespace yalahoo {
AudioPreferencesBinding::AudioPreferencesBinding(PreferencesStore &preferences,
                                                 PlaybackController &playback,
                                                 BackgroundMusicController &background,
                                                 InterfaceSoundController &sounds, QObject *parent)
    : QObject(parent),
      m_preferences(preferences),
      m_playback(playback),
      m_background(background),
      m_sounds(sounds)
{
    apply();
    connect(&preferences, &PreferencesStore::imported, this, &AudioPreferencesBinding::apply);
    connect(&playback, &PlaybackController::volumeChanged, this, &AudioPreferencesBinding::persist);
    connect(&background, &BackgroundMusicController::settingsChanged, this,
            &AudioPreferencesBinding::persist);
    connect(&sounds, &InterfaceSoundController::settingsChanged, this,
            &AudioPreferencesBinding::persist);
}
void AudioPreferencesBinding::apply()
{
    const QScopedValueRollback applying(m_applying, true);
    const auto audio = m_preferences.audio();
    m_playback.setVolume(audio.value(u"volume"_s).toDouble(productDefaults::DefaultSongVolume));
    m_background.setVolume(
            audio.value(u"bgmVolume"_s).toDouble(productDefaults::DefaultBackgroundVolume));
    m_background.setEnabled(audio.value(u"bgm"_s).toBool(true));
    m_sounds.setVolume(
            audio.value(u"soundVolume"_s).toDouble(productDefaults::DefaultFeedbackVolume));
    m_sounds.setEnabled(audio.value(u"sound"_s).toBool(true));
}
void AudioPreferencesBinding::persist()
{
    if (m_applying)
        return;
    m_preferences.setAudio({ { u"volume"_s, m_playback.volume() },
                             { u"bgm"_s, m_background.enabled() },
                             { u"bgmVolume"_s, m_background.volume() },
                             { u"sound"_s, m_sounds.enabled() },
                             { u"soundVolume"_s, m_sounds.volume() } });
}
}
