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

namespace yalahoo {
class AudioAmbienceTest : public QObject
{
    Q_OBJECT
private slots:
    void realBgmGateInterruptionsAndHiddenPlayback()
    {
        QTemporaryDir root;
        QFile file(root.filePath("anonymous.wav"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write(test::wave(2500)) > 0);
        file.close();
        OfflineMetadataProvider provider;
        LibrarySession library(&provider);
        Album album;
        album.id = "album";
        Track track;
        track.id = "track";
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
        QTRY_VERIFY_WITH_TIMEOUT(background.playing(), 10000);
        QTRY_VERIFY(!background.m_fade.isActive());
        QVERIFY(background.error().isEmpty());
        QCOMPARE(background.m_player.loops(), QMediaPlayer::Infinite);
        QCOMPARE(background.m_player.source(), QUrl("qrc:/assets/audio/atmosphere.ogg"));
        QCOMPARE(background.m_output.volume(), .01f);
        const auto duration = background.m_player.duration();
        QVERIFY(duration > 1000);
        background.m_player.setPosition(duration - 50);
        QTRY_VERIFY_WITH_TIMEOUT(background.m_player.position() < duration / 2, 5000);
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
        QTest::qWait(80);
        QVERIFY(!song.playing());
        QTRY_VERIFY_WITH_TIMEOUT(song.playing(), 10000);
        QVERIFY(elapsed.elapsed() >= 220);
        QVERIFY(background.silent());
        QVERIFY(!overlap);
        QVERIFY(!background.m_fade.isActive());
        song.pause();
        QTest::qWait(700);
        QVERIFY(background.silent());
        QVERIFY(!background.playing());
        // No window or visual clock exists: ordinary media progress continues while hidden.
        song.toggle();
        QTRY_VERIFY(song.playing());
        const auto before = song.position();
        library.search("other");
        library.navigate(album.id);
        QTRY_VERIFY(song.position() > before + 100);
        QCOMPARE(song.trackId(), track.id);
        QSignalSpy ended(&song, &PlaybackController::queueFinished);
        QTRY_COMPARE_WITH_TIMEOUT(ended.size(), 1, 10000);
        QTRY_VERIFY(!background.m_fade.isActive());
        QTRY_VERIFY(background.playing());
        QCOMPARE(background.m_output.volume(), .01f);
        song.toggle();
        song.pause();
        QTest::qWait(300);
        QVERIFY(background.silent());
        QVERIFY(!song.playing());
        song.stop();
        QTest::qWait(100);
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
        QTRY_VERIFY_WITH_TIMEOUT(background.playing(), 10000);
        QTemporaryDir root;
        background.m_player.setSource(QUrl::fromLocalFile(root.filePath("missing.ogg")));
        QTRY_VERIFY_WITH_TIMEOUT(!background.error().isEmpty(), 10000);
        QVERIFY(background.silent());
        QVERIFY(!background.m_fade.isActive());
        background.setSongActivity(BackgroundMusicController::SongActivity::Playing);
        background.setSongActivity(BackgroundMusicController::SongActivity::Stopped);
        QVERIFY(!background.playing());
        background.setEnabled(false);
        background.setEnabled(true);
        QTRY_VERIFY_WITH_TIMEOUT(background.playing(), 10000);
        QVERIFY(background.error().isEmpty());
        QCOMPARE(background.m_player.source(), QUrl("qrc:/assets/audio/atmosphere.ogg"));
        background.stop();
    }
    void missingEventsAreSilentAndPoolIsBounded()
    {
        InterfaceSoundController sounds;
        BackgroundMusicController background;
        QFile manifest(":/assets/audio/interface-events.json");
        QVERIFY(manifest.open(QIODevice::ReadOnly));
        const auto events =
                QJsonDocument::fromJson(manifest.readAll()).object()["events"].toArray();
        QCOMPARE(events.size(), 17);
        QVERIFY(!sounds.available());
        for (const auto &event : events)
            QVERIFY(!sounds.trigger(event.toObject()["event"].toString()));
        for (int i = 0; i < 10000; ++i)
            QVERIFY(!sounds.trigger("key"));
        for (const auto &effect : sounds.m_pool)
            QVERIFY(!effect);
        const auto original = sounds.volume();
        sounds.setVolume(std::numeric_limits<double>::quiet_NaN());
        QCOMPARE(sounds.volume(), original);
        sounds.setVolume(2);
        QCOMPARE(sounds.volume(), 1.);
        QCOMPARE(background.volume(), .18);
        sounds.setEnabled(false);
        QVERIFY(!sounds.trigger("confirm"));
        sounds.setEnabled(true);
        // Test-only short WAV exercises the same production QSoundEffect dispatch.
        QTemporaryDir root;
        QFile file(root.filePath("short.wav"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write(test::wave(1000)) > 0);
        file.close();
        const auto url = QUrl::fromLocalFile(file.fileName());
        sounds.m_sources.insert("confirm", url);
        sounds.setVolume(.01);
        for (auto &effect : sounds.m_pool) {
            effect = std::make_unique<QSoundEffect>();
            effect->setAudioDevice(QMediaDevices::defaultAudioOutput());
            effect->setVolume(.01f);
            effect->setSource(url);
        }
        for (const auto &effect : sounds.m_pool)
            QTRY_COMPARE_WITH_TIMEOUT(effect->status(), QSoundEffect::Ready, 10000);
        for (int i = 0; i < 4; ++i)
            QVERIFY(sounds.trigger("confirm"));
        QVERIFY(!sounds.trigger("confirm"));
        sounds.setEnabled(false);
        for (const auto &effect : sounds.m_pool)
            QVERIFY(!effect->isPlaying());
    }
    void audioPreferencesPersistIndependently()
    {
        QTemporaryDir root;
        const QJsonObject audio{ { "volume", .5 },
                                 { "bgm", false },
                                 { "bgmVolume", .3 },
                                 { "sound", true },
                                 { "soundVolume", .4 } };
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
        QFile imported(root.filePath("import.json"));
        QVERIFY(imported.open(QIODevice::WriteOnly));
        imported.write(R"({"bgm":true})");
        imported.close();
        QSignalSpy importedSignal(&prefs, &PreferencesStore::imported);
        QSignalSpy savedSignal(&prefs, &PreferencesStore::saved);
        QVERIFY(prefs.importJson(QUrl::fromLocalFile(imported.fileName())));
        QTRY_COMPARE(importedSignal.size(), 1);
        QTRY_VERIFY(!savedSignal.isEmpty());
        QCOMPARE(prefs.audio()["bgm"].toBool(), true);
        QCOMPARE(prefs.audio()["volume"].toDouble(), .5);
        QCOMPARE(prefs.audio()["soundVolume"].toDouble(), .4);
    }
};
}
QTEST_GUILESS_MAIN(yalahoo::AudioAmbienceTest)
#include "AudioAmbienceTest.moc"
