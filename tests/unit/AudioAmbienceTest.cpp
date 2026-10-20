// Checks the independent song, background and feedback routes using generated anonymous media.
// Explicit fixture literals are independent inputs or expected results; timing bounds have named
// constants.

#include "LibrarySession.h"
#include "BackgroundMusicController.h"
#include "InterfaceSoundController.h"
#include "PlaybackController.h"
#include "PreferencesStore.h"
#include "OfflineMetadataProvider.h"
#include "AudioFixture.h"
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <limits>

namespace {
// Bounds for asynchronous completion and event delivery; fixture values remain independent.
constexpr int AsyncDeadlineMilliseconds = 10000;
constexpr int BackendDeadlineMilliseconds = 5000;
constexpr int MuteObservationMilliseconds = 80;
constexpr int FadeCompletionMilliseconds = 700;
constexpr int TeardownObservationMilliseconds = 300;
constexpr int QueuedEventObservationMilliseconds = 100;
}

using namespace Qt::StringLiterals;

namespace yalahoo {
class AudioAmbienceTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AudioAmbienceTest)
public:
    AudioAmbienceTest() = default;

private Q_SLOTS:
    void realBgmGateInterruptionsAndHiddenPlayback()
    {
        QTemporaryDir root;
        QFile file(root.filePath(u"anonymous.wav"_s));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write(test::wave(2500)) > 0);
        file.close();
        OfflineMetadataProvider provider;
        LibrarySession library(&provider);
        Album album;
        album.id = "album"_L1;
        Track track;
        track.id = "track"_L1;
        track.albumId = album.id;
        track.sourceUrl = QUrl::fromLocalFile(file.fileName());
        album.tracks.append(track);
        LibrarySnapshot snapshot;
        snapshot.albums.append(album);
        library.setSnapshot(snapshot);
        library.navigate(album.id);
        BackgroundMusicController background;
        PlaybackController song(&library);
        song.setBackgroundMusic(&background);
        song.setVolume(.01);
        background.setVolume(.01);
        QVERIFY(song.outputAvailable());
        background.start();
        QTRY_VERIFY_WITH_TIMEOUT(background.playing(), AsyncDeadlineMilliseconds);
        QTRY_VERIFY(!background.m_fade.isActive());
        QVERIFY(background.error().isEmpty());
        QCOMPARE(background.m_player.loops(), QMediaPlayer::Infinite);
        QCOMPARE(background.m_player.source(), QUrl(u"qrc:/assets/audio/atmosphere.ogg"_s));
        QCOMPARE(background.m_output.volume(), .01f);
        const auto duration = background.m_player.duration();
        QVERIFY(duration > 1000);
        background.m_player.setPosition(duration - 50);
        QTRY_VERIFY_WITH_TIMEOUT(background.m_player.position() < duration / 2,
                                 BackendDeadlineMilliseconds);
        QVERIFY(background.playing());
        bool overlap = false;
        connect(&song, &PlaybackController::stateChanged, &song, [&] {
            if (song.playing() && !background.silent())
                overlap = true;
        });
        QElapsedTimer elapsed;
        elapsed.start();
        song.toggle();
        QVERIFY(song.requested());
        QVERIFY(!song.playing());
        QTest::qWait(MuteObservationMilliseconds);
        QVERIFY(!song.playing());
        QTRY_VERIFY_WITH_TIMEOUT(song.playing(), AsyncDeadlineMilliseconds);
        QVERIFY(elapsed.elapsed() >= 220);
        QVERIFY(background.silent());
        QVERIFY(!overlap);
        QVERIFY(!background.m_fade.isActive());
        song.pause();
        QTest::qWait(FadeCompletionMilliseconds);
        QVERIFY(background.silent());
        QVERIFY(!background.playing());
        // No window or visual clock exists: ordinary media progress continues while hidden.
        song.toggle();
        QTRY_VERIFY(song.playing());
        const auto before = song.position();
        library.search(u"other"_s);
        library.navigate(album.id);
        QTRY_VERIFY(song.position() > before + 100);
        QCOMPARE(song.trackId(), track.id);
        QSignalSpy ended(&song, &PlaybackController::queueFinished);
        QTRY_COMPARE_WITH_TIMEOUT(ended.size(), 1, AsyncDeadlineMilliseconds);
        QTRY_VERIFY(!background.m_fade.isActive());
        QTRY_VERIFY(background.playing());
        QCOMPARE(background.m_output.volume(), .01f);
        song.toggle();
        song.pause();
        QTest::qWait(TeardownObservationMilliseconds);
        QVERIFY(background.silent());
        QVERIFY(!song.playing());
        song.stop();
        QTest::qWait(QueuedEventObservationMilliseconds);
        QVERIFY(background.m_fade.isActive());
        song.toggle();
        song.stop();
        background.setEnabled(false);
        QTRY_VERIFY(!background.m_fade.isActive());
        QVERIFY(background.silent());
        background.setVolume(.02);
        QCOMPARE(song.volume(), double(.01f));
        background.setEnabled(true);
        QTRY_VERIFY(!background.m_fade.isActive());
        QCOMPARE(background.m_output.volume(), .02f);
        background.stop();
        QVERIFY(!background.m_fade.isActive());
        QVERIFY(background.silent());
        QVERIFY(!background.playing());
    }
    void bgmErrorIsSilentAndExplicitEnableRetries()
    {
        BackgroundMusicController background;
        background.setVolume(.01);
        background.start();
        QTRY_VERIFY_WITH_TIMEOUT(background.playing(), AsyncDeadlineMilliseconds);
        QTemporaryDir root;
        background.m_player.setSource(QUrl::fromLocalFile(root.filePath(u"missing.ogg"_s)));
        QTRY_VERIFY_WITH_TIMEOUT(!background.error().isEmpty(), AsyncDeadlineMilliseconds);
        QVERIFY(background.silent());
        QVERIFY(!background.m_fade.isActive());
        background.setSongActivity(BackgroundMusicController::SongActivity::Playing);
        background.setSongActivity(BackgroundMusicController::SongActivity::Stopped);
        QVERIFY(!background.playing());
        background.setEnabled(false);
        background.setEnabled(true);
        QTRY_VERIFY_WITH_TIMEOUT(background.playing(), AsyncDeadlineMilliseconds);
        QVERIFY(background.error().isEmpty());
        QCOMPARE(background.m_player.source(), QUrl(u"qrc:/assets/audio/atmosphere.ogg"_s));
        background.stop();
    }
    void missingEventsAreSilentAndPoolIsBounded()
    {
        InterfaceSoundController sounds;
        BackgroundMusicController background;
        QFile manifest(u":/assets/audio/interface-events.json"_s);
        QVERIFY(manifest.open(QIODevice::ReadOnly));
        const auto events =
                QJsonDocument::fromJson(manifest.readAll()).object()[u"events"_s].toArray();
        QCOMPARE(events.size(), 17);
        QVERIFY(!sounds.available());
        for (const auto &event : events)
            QVERIFY(!sounds.trigger(event.toObject()[u"event"_s].toString()));
        for (int i = 0; i < 10000; ++i)
            QVERIFY(!sounds.trigger(u"key"_s));
        for (const auto &effect : sounds.m_pool)
            QVERIFY(!effect);
        const auto original = sounds.volume();
        sounds.setVolume(std::numeric_limits<double>::quiet_NaN());
        QCOMPARE(sounds.volume(), original);
        sounds.setVolume(2);
        QCOMPARE(sounds.volume(), 1.);
        QCOMPARE(background.volume(), .18);
        sounds.setEnabled(false);
        QVERIFY(!sounds.trigger(u"confirm"_s));
        sounds.setEnabled(true);
        // Test-only short WAV exercises the same production QSoundEffect dispatch.
        QTemporaryDir root;
        QFile file(root.filePath(u"short.wav"_s));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write(test::wave(1000)) > 0);
        file.close();
        const auto url = QUrl::fromLocalFile(file.fileName());
        sounds.m_sources.insert(u"confirm"_s, url);
        sounds.setVolume(.01);
        for (auto &effect : sounds.m_pool) {
            effect = std::make_unique<QSoundEffect>();
            effect->setAudioDevice(QMediaDevices::defaultAudioOutput());
            effect->setVolume(.01f);
            effect->setSource(url);
        }
        for (const auto &effect : sounds.m_pool)
            QTRY_COMPARE_WITH_TIMEOUT(effect->status(), QSoundEffect::Ready,
                                      AsyncDeadlineMilliseconds);
        for (int i = 0; i < 4; ++i)
            QVERIFY(sounds.trigger(u"confirm"_s));
        QVERIFY(!sounds.trigger(u"confirm"_s));
        sounds.setEnabled(false);
        for (const auto &effect : sounds.m_pool)
            QVERIFY(!effect->isPlaying());
    }
    void audioPreferencesPersistIndependently()
    {
        QTemporaryDir root;
        const QJsonObject audio{ { u"volume"_s, .5 },
                                 { u"bgm"_s, false },
                                 { u"bgmVolume"_s, .3 },
                                 { u"sound"_s, true },
                                 { u"soundVolume"_s, .4 } };
        {
            PreferencesStore prefs(root.path());
            prefs.setNavigation(false, true);
            for (int i = 0; i < 20; ++i)
                prefs.setAudio(audio);
        }
        PreferencesStore prefs(root.path());
        QCOMPARE(prefs.audio(), audio);
        QVERIFY(prefs.realistic());
        QVERIFY(!prefs.rememberColumnPosition());
        QFile imported(root.filePath(u"import.json"_s));
        QVERIFY(imported.open(QIODevice::WriteOnly));
        imported.write(R"({"bgm":true})"_ba);
        imported.close();
        QSignalSpy importedSignal(&prefs, &PreferencesStore::imported);
        QSignalSpy savedSignal(&prefs, &PreferencesStore::saved);
        QVERIFY(prefs.importJson(QUrl::fromLocalFile(imported.fileName())));
        QTRY_COMPARE(importedSignal.size(), 1);
        QTRY_VERIFY(!savedSignal.isEmpty());
        QCOMPARE(prefs.audio()[u"bgm"_s].toBool(), true);
        QCOMPARE(prefs.audio()[u"volume"_s].toDouble(), .5);
        QCOMPARE(prefs.audio()[u"soundVolume"_s].toDouble(), .4);
    }
};
}
QTEST_GUILESS_MAIN(yalahoo::AudioAmbienceTest)
#include "AudioAmbienceTest.moc"
