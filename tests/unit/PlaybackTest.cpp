#include "PlaybackController.h"
#include "AudioFixture.h"
#include "OfflineMetadataProvider.h"
#include <QDataStream>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <cmath>

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {

struct Fixture
{
    QTemporaryDir directory;
    OfflineMetadataProvider provider;
    LibrarySession library{ &provider };
    PlaybackController player{ &library };
    Fixture()
    {
        LibrarySnapshot snapshot;
        Album album;
        album.id = "album"_L1;
        for (int i = 0; i < 3; ++i) {
            const auto path = directory.path() + u"/\u6b4c\u66f2 #%1.wav"_s.arg(i);
            QFile file(path);
            if (file.open(QIODevice::WriteOnly))
                file.write(i == 2 ? "unsupported anonymous file"_ba
                                  : test::wave(i == 0 ? 5000 : 400));
            Track track;
            track.id = u"track-%1"_s.arg(i);
            track.albumId = album.id;
            track.title = u"Song %1"_s.arg(i);
            track.sourceUrl = QUrl::fromLocalFile(path);
            album.tracks.append(track);
        }
        snapshot.albums.append(album);
        library.setSnapshot(snapshot);
        library.navigate(u"album"_s);
        player.setVolume(.01);
    }
};
}
class PlaybackTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void publicPlaybackPauseSeekStopAndNavigation()
    {
        Fixture f;
        QVERIFY2(f.player.outputAvailable(),
                 "No real output device; native playback gate remains unverified");
        QVERIFY(f.player.isAvailable());
        f.player.toggle();
        QTRY_VERIFY_WITH_TIMEOUT(f.player.playing(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(f.player.position() > 100, 5000);
        QCOMPARE(f.player.mediaSupport(), MediaSupport::Supported);
        QVERIFY(!f.player.playTrack(u"missing"_s, u"track-0"_s));
        QVERIFY(f.player.playing());
        QVERIFY(f.player.error().isEmpty());
        QCOMPARE(f.library.playbackTrackId(), u"track-0"_s);
        const auto revision = f.player.sourceRevision();
        f.library.search(u"missing"_s);
        f.library.sort(u"artist"_s);
        f.library.navigate(u"album"_s, u"track-1"_s);
        QCOMPARE(f.player.sourceRevision(), revision);
        QCOMPARE(f.player.trackId(), u"track-0"_s);
        f.player.pause();
        QTRY_VERIFY(!f.player.playing());
        QVERIFY(!f.player.requested());
        QTRY_VERIFY(f.player.seekable());
        f.player.seek(1000);
        QTRY_VERIFY(std::abs(f.player.position() - 1000) < 100);
        f.player.beginSeek();
        f.player.previewSeek(2000);
        QCOMPARE(f.player.position(), 2000);
        // A live backend update cannot overwrite the thumb while dragging.
        QCOMPARE(f.player.m_player.position(), 1000);
        f.player.m_player.setPosition(1200);
        QCOMPARE(f.player.position(), 2000);
        f.player.commitSeek();
        QTRY_VERIFY(std::abs(f.player.m_player.position() - 2000) < 100);
        f.player.toggle();
        QTRY_VERIFY(f.player.playing());
        f.player.previous();
        QTRY_VERIFY(f.player.position() < 1000);
        f.player.pause();
        f.player.seek(4000);
        f.player.previous();
        QVERIFY(!f.player.requested());
        QCOMPARE(f.player.position(), 0);
        f.player.stop();
        QVERIFY(f.player.source().isEmpty());
        QCOMPARE(f.player.position(), 0);
        QVERIFY(!f.player.seekable());
        QVERIFY(!f.player.requested());
        f.player.toggle();
        f.player.pause();
        QTRY_VERIFY_WITH_TIMEOUT(f.player.m_player.mediaStatus() == QMediaPlayer::LoadedMedia
                                         || f.player.m_player.mediaStatus()
                                                 == QMediaPlayer::BufferedMedia,
                                 10000);
        QVERIFY(!f.player.playing());
        QCOMPARE(f.player.m_player.playbackRate(), 1.);
    }
    void lateLoadEndAndQueueOwnership()
    {
        Fixture f;
        QVERIFY(f.player.outputAvailable());
        QVERIFY(f.player.playTrack(u"album"_s, u"track-0"_s));
        const auto old = f.player.sourceRevision();
        f.player.next();
        const auto current = f.player.sourceRevision();
        QVERIFY(current > old);
        QCOMPARE(f.player.trackId(), u"track-1"_s);
        f.player.status(QMediaPlayer::EndOfMedia, old);
        QCOMPARE(f.player.trackId(), u"track-1"_s);
        f.player.pause();
        f.player.status(QMediaPlayer::LoadedMedia, current);
        QVERIFY(!f.player.requested());
        f.player.stop();
        f.player.status(QMediaPlayer::LoadedMedia, current);
        QVERIFY(f.player.source().isEmpty());
        QVERIFY(!f.player.requested());
        QVERIFY(f.player.playTrack(u"album"_s, u"track-0"_s));
        f.player.pause();
        f.library.setSnapshot({ });
        f.player.next();
        QCOMPARE(f.player.trackId(), u"track-1"_s);
        QCOMPARE(f.library.playbackTrackId(), u"track-1"_s);
        // Exactly one end commit for the current source, despite duplicate notifications.
        f.player.status(QMediaPlayer::EndOfMedia, f.player.sourceRevision());
        f.player.status(QMediaPlayer::EndOfMedia, f.player.sourceRevision());
        QTRY_COMPARE(f.player.trackId(), u"track-2"_s);
        QTRY_VERIFY_WITH_TIMEOUT(!f.player.error().isEmpty(), 10000);
        QVERIFY(!f.player.playing());
        f.player.stop();
        QVERIFY(f.player.error().isEmpty());
    }
    void outputLossPendingIntentAndExplicitSelection()
    {
        Fixture f;
        const auto real = QMediaDevices::audioOutputs();
        const auto systemDefault = QMediaDevices::defaultAudioOutput();
        QVERIFY(!real.isEmpty());
        QVERIFY(!systemDefault.isNull());
        f.player.applyDevices({ }, { });
        QVERIFY(!f.player.outputAvailable());
        QVERIFY(f.player.playTrack(u"album"_s, u"track-0"_s));
        QVERIFY(f.player.requested());
        QVERIFY(f.player.source().isEmpty());
        f.player.pause();
        f.player.applyDevices(real, systemDefault);
        QTRY_VERIFY_WITH_TIMEOUT(f.player.m_player.mediaStatus() == QMediaPlayer::LoadedMedia
                                         || f.player.m_player.mediaStatus()
                                                 == QMediaPlayer::BufferedMedia,
                                 10000);
        QVERIFY(!f.player.playing());
        QVERIFY(!f.player.requested());
        f.player.toggle();
        QTRY_VERIFY_WITH_TIMEOUT(f.player.playing(), 10000);
        QTRY_VERIFY(f.player.seekable());
        f.player.seek(1500);
        f.player.pause();
        // Model a backend's spurious zero before the output-loss notification.
        f.player.m_player.setPosition(0);
        f.player.applyDevices({ }, { });
        f.player.applyDevices(real, systemDefault);
        QTRY_VERIFY(std::abs(f.player.position() - 1500) < 100);
        QVERIFY(!f.player.playing());
        QCOMPARE(f.player.volume(), double(.01f));
        f.player.toggle();
        QTRY_VERIFY(f.player.playing());
        f.player.applyDevices({ }, { });
        QVERIFY(f.player.requested());
        f.player.applyDevices(real, systemDefault);
        QTRY_VERIFY_WITH_TIMEOUT(f.player.playing(), 10000);
        f.player.pause();
        const auto id = QString::fromUtf8(real.first().id().toBase64());
        QVERIFY(f.player.selectDevice(id));
        QCOMPARE(f.player.selectedDevice(), id);
        f.player.applyDevices({ }, { });
        QVERIFY(f.player.selectedDevice().isEmpty());
        QVERIFY(!f.player.notice().isEmpty());
        f.player.stop();
        f.player.applyDevices(real, systemDefault);
        QVERIFY(!f.player.playing());
        QVERIFY(f.player.source().isEmpty());
        f.player.applyDevices({ }, { });
        QVERIFY(f.player.playTrack(u"album"_s, u"track-1"_s));
        f.player.stop();
        f.player.applyDevices(real, systemDefault);
        QVERIFY(f.player.source().isEmpty());
        QVERIFY(!f.player.requested());
        QVERIFY(!f.player.selectDevice(u"not-a-device"_s));
        QVERIFY(f.player.selectDevice(u""_s));
    }
    void realEndAndUnsupportedFormat()
    {
        Fixture f;
        QVERIFY(f.player.outputAvailable());
        QSignalSpy changed(&f.player, &PlaybackController::currentChanged);
        QVERIFY(f.player.playTrack(u"album"_s, u"track-1"_s));
        QTRY_COMPARE_WITH_TIMEOUT(f.player.trackId(), u"track-2"_s, 10000);
        QCOMPARE(changed.size(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(!f.player.error().isEmpty(), 10000);
        QVERIFY(!f.player.playing());
        f.player.retry();
        QTRY_VERIFY_WITH_TIMEOUT(!f.player.error().isEmpty(), 10000);
        f.player.stop();
        QVERIFY(!f.player.seekable());
        f.player.seek(2000);
        QCOMPARE(f.player.position(), 0);
        QVERIFY(!f.player.playTrack(u"missing"_s, u"track-0"_s));
        QCOMPARE(f.player.m_player.playbackRate(), 1.);
    }
};
}
QTEST_GUILESS_MAIN(yalahoo::PlaybackTest)
#include "PlaybackTest.moc"
