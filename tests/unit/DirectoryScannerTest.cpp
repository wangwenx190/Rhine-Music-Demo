// Uses temporary directory trees to verify stable identity, cancellation and transactional scan
// publication. Explicit fixture literals are independent inputs or expected results; timing
// bounds have named constants.

#include "../support/FileFixture.h"
#include "DirectoryScanner.h"
#include "LibrarySession.h"
#include "LibraryStore.h"
#include "OfflineMetadataProvider.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <filesystem>

using namespace Qt::StringLiterals;

using namespace yalahoo;
namespace {
// Bounds for asynchronous completion and event delivery; fixture values remain independent.
constexpr int AsyncDeadlineMilliseconds = 10000;
constexpr int LargeScanDeadlineMilliseconds = 20000;
constexpr int EventPollMilliseconds = 10;

bool put(const QString &path, const QByteArray &bytes = "anonymous audio fixture"_ba)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
Snapshot scan(const QString &root, Snapshot previous = { })
{
    return DirectoryScanner::scan({ QUrl::fromLocalFile(root) }, previous,
                                  std::make_shared<std::atomic_bool>(false))
            .snapshot;
}
const Album *atFolder(Snapshot snapshot, const QString &folder)
{
    for (const auto &album : snapshot->albums)
        if (album.folder.toLocalFile() == folder)
            return &album;
    return nullptr;
}
}
class DirectoryScannerTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DirectoryScannerTest)
public:
    DirectoryScannerTest() = default;

private Q_SLOTS:
    void rootValidation();
    void panelRootPaths();
    void layoutAndIdentity();
    void symlinks();
    void fingerprintAndReuse();
    void rescanRenameDeleteOffline();
    void partialFailure();
    void cancellation();
    void storeRoundTripAndResponsiveness();
    void latestRootAndCancelledResult();
    void badCacheAndWriteFailure();
    void atomicSaveFailure();
    void destruction();
};
void DirectoryScannerTest::panelRootPaths()
{
    QTemporaryDir temporary;
    const QString music = temporary.path() + u"/\u97f3\u4e50 \u76ee\u5f55 #1"_s;
    QVERIFY(QDir().mkpath(music));
    LibraryStore store(temporary.path() + "/data"_L1);
    QVERIFY(!store.setRootPaths(u"relative/path"_s));
    QVERIFY(store.setRootPaths(u"\n"_s + music + u"\n"_s + music + u"\n"_s));
    QCOMPARE(store.rootPaths(), QStringList{ music });
    QCOMPARE(store.localPath(QUrl::fromLocalFile(music)), music);
    QTRY_VERIFY_WITH_TIMEOUT(!store.busy(), AsyncDeadlineMilliseconds);
    QCOMPARE(store.roots().size(), 1);
    QVERIFY(store.setRootPaths(u""_s));
    QTRY_VERIFY_WITH_TIMEOUT(!store.busy(), AsyncDeadlineMilliseconds);
    QVERIFY(store.rootPaths().isEmpty());
}
void DirectoryScannerTest::rootValidation()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto root = temporary.path();
    QList<QUrl> output{ QUrl::fromLocalFile(root) };
    QString error;
    const auto nested = QUrl::fromLocalFile(root + "/child"_L1);
    QVERIFY(DirectoryScanner::normalizeRoots(
            { nested, QUrl::fromLocalFile(root + u"/./"_s), QUrl::fromLocalFile(root) }, output,
            error));
    QCOMPARE(output, QList<QUrl>{ QUrl::fromLocalFile(root) });
    for (const auto &invalid : { QUrl(u"https://example.invalid/music"_s), QUrl(u"relative"_s),
                                 QUrl::fromLocalFile(u"/bad"_s + QChar::Null) }) {
        QVERIFY(!DirectoryScanner::normalizeRoots({ invalid }, output, error));
        QCOMPARE(output, QList<QUrl>{ QUrl::fromLocalFile(root) });
    }
    QList<QUrl> many;
    for (int i = 0; i < 65; ++i)
        many.append(QUrl::fromLocalFile(root + QString::number(i)));
    QVERIFY(!DirectoryScanner::normalizeRoots(many, output, error));
    many.removeLast();
    QVERIFY(DirectoryScanner::normalizeRoots(many, output, error));
    QCOMPARE(output.size(), 64);
    QVERIFY(DirectoryScanner::normalizeRoots({ }, output, error));
    QVERIFY(output.isEmpty());
}
void DirectoryScannerTest::layoutAndIdentity()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString root = temporary.path() + u"/\u97f3\u4e50 \u7a7a\u683c # %"_s;
    const QString album = root + u"/\u4e13\u8f91"_s;
    const QStringList extensions{ u"ape"_s,  u"wv"_s,   u"wma"_s, u"flac"_s, u"wav"_s, u"m4a"_s,
                                  u"mp4"_s,  u"alac"_s, u"dsf"_s, u"dff"_s,  u"mp3"_s, u"aac"_s,
                                  u"aiff"_s, u"aif"_s,  u"ogg"_s, u"opus"_s };
    for (const auto &ext : extensions)
        QVERIFY(put(album + u"/\u66f2\u76ee."_s + ext.toUpper()));
    QVERIFY(put(root + u"/\u76f4\u5c5e1.mp3"_s));
    QVERIFY(put(root + u"/\u76f4\u5c5e2.flac"_s));
    QVERIFY(put(root + u"/\u591a CD/CD1/1.wav"_s));
    QVERIFY(put(root + u"/\u591a CD/CD2/2.wav"_s));
    QVERIFY(put(root + u"/.\u5ffd\u7565/\u9690\u85cf.mp3"_s));
    QVERIFY(put(album + u"/ignored.txt"_s));
    QVERIFY(put(album + u"/.\u53ef\u7d22\u5f15.mp3"_s));
    const auto result = scan(root);
    QVERIFY(result);
    QCOMPARE(result->albums.size(), 5);
    QCOMPARE(atFolder(result, album)->tracks.size(), 17);
    QCOMPARE(result->roots.first().status, u"online"_s);
    int singles = 0;
    for (const auto &entry : result->albums) {
        const bool single = entry.fields.value(u"_singleFile"_s).toBool();
        singles += single;
        const QString identity =
                single ? entry.tracks.first().sourceUrl.toLocalFile() : entry.folder.toLocalFile();
        const auto hash = QCryptographicHash::hash(QDir::toNativeSeparators(identity).toUtf8(),
                                                   QCryptographicHash::Sha256)
                                  .toHex()
                                  .first(24);
        QCOMPARE(entry.id, u"album-"_s + QString::fromUtf8(hash));
        for (const auto &track : entry.tracks) {
            QCOMPARE(track.albumId, entry.id);
            QCOMPARE(track.mediaSupport, MediaSupport::Unknown);
            QCOMPARE(track.sourceUrl.toLocalFile(), root + '/'_L1 + track.relativePath);
            QVERIFY(!track.sourceUrl.hasQuery());
            QVERIFY(!track.sourceUrl.hasFragment());
        }
    }
    QCOMPARE(singles, 2);
}
void DirectoryScannerTest::symlinks()
{
    QTemporaryDir temporary;
    const QString root = temporary.path() + "/root"_L1;
    QVERIFY(put(root + u"/album/file.mp3"_s));
    QVERIFY(put(temporary.path() + u"/outside/file.mp3"_s));
    const auto native = [](const QString &path) {
#ifdef Q_OS_WIN
        return std::filesystem::path(path.toStdWString());
#else
        return std::filesystem::path(path.toUtf8().toStdString());
#endif
    };
    std::error_code error;
    std::filesystem::create_directory_symlink(native(root), native(root + "/album/cycle"_L1),
                                              error);
#ifdef Q_OS_WIN
    if (error)
        QSKIP("Native symlink creation unavailable; Windows symlink/junction checks remain R21-P.");
#endif
    QVERIFY2(!error, error.message().c_str());
    std::filesystem::create_directory_symlink(native(root + "/album"_L1),
                                              native(root + "/duplicate"_L1), error);
    QVERIFY2(!error, error.message().c_str());
    std::filesystem::create_directory_symlink(native(temporary.path() + "/outside"_L1),
                                              native(root + "/external"_L1), error);
    QVERIFY2(!error, error.message().c_str());
    std::filesystem::create_symlink(native(root + "/album/file.mp3"_L1),
                                    native(root + "/alias.mp3"_L1), error);
    QVERIFY2(!error, error.message().c_str());
    const auto result = scan(root);
    QCOMPARE(result->albums.size(), 1);
    QCOMPARE(result->albums.first().tracks.size(), 1);
    QCOMPARE(scan(root + u"/duplicate"_s)->roots.first().status, u"offline"_s);
}
void DirectoryScannerTest::fingerprintAndReuse()
{
    QTemporaryDir temporary;
    const QString path = temporary.path() + u"/album/1-02 \u6b4c\u66f2.FLAC"_s;
    QVERIFY(put(path, "abc"_ba));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadWrite));
    QVERIFY(file.setFileTime(QDateTime::fromMSecsSinceEpoch(1700000000123, QTimeZone::UTC),
                             QFileDevice::FileModificationTime));
    file.close();
    QCOMPARE(DirectoryScanner::fingerprint(path), u"3:1700000000123"_s);
    const auto originalHash =
            QCryptographicHash::hash(yalahoo::test::readFile(path), QCryptographicHash::Sha256);
    auto first = scan(temporary.path());
    QVERIFY(first);
    auto previous = *first;
    previous.fields.insert(u"futureLibraryField"_s, 17);
    previous.scan.insert(u"futureScanField"_s, u"retained"_s);
    auto &album = previous.albums.first();
    album.title = "metadata album"_L1;
    album.localNote = "local note"_L1;
    album.fields.insert(u"futureAlbumField"_s, 42);
    auto &track = album.tracks.first();
    QCOMPARE(track.discNumber, std::optional<int>(1));
    QCOMPARE(track.trackNumber, std::optional<int>(2));
    track.title = "metadata title"_L1;
    track.fields.insert(u"_qtProbeVersion"_s, QString::fromUtf8(qVersion()));
    track.fields.insert(u"futureTrackField"_s, u"retained"_s);
    track.mediaSupport = MediaSupport::Supported;
    auto second = scan(temporary.path(), std::make_shared<const LibrarySnapshot>(previous));
    QCOMPARE(second->albums.first().tracks.first().title, u"metadata title"_s);
    QCOMPARE(second->albums.first().localNote, u"local note"_s);
    QCOMPARE(second->fields.value(u"futureLibraryField"_s).toInt(), 17);
    QCOMPARE(second->scan.value(u"futureScanField"_s).toString(), u"retained"_s);
    QCOMPARE(second->genres.first().albumCount, std::optional<int>(1));
    QCOMPARE(second->albums.first().fields.value(u"futureAlbumField"_s).toInt(), 42);
    QCOMPARE(QCryptographicHash::hash(yalahoo::test::readFile(path), QCryptographicHash::Sha256),
             originalHash);
    QVERIFY(put(path, "changed source fixture"_ba));
    auto third = scan(temporary.path(), second);
    const auto &changed = third->albums.first().tracks.first();
    QCOMPARE(changed.id, track.id);
    QCOMPARE(changed.title, u"1-02 \u6b4c\u66f2"_s);
    QCOMPARE(changed.mediaSupport, MediaSupport::Unknown);
    QVERIFY(!changed.fields.contains(u"_qtProbeVersion"_s));
    QCOMPARE(changed.fields.value(u"futureTrackField"_s).toString(), u"retained"_s);
    QCOMPARE(second->albums.first().tracks.first().title, u"metadata title"_s);
}
void DirectoryScannerTest::rescanRenameDeleteOffline()
{
    QTemporaryDir temporary;
    const QString root = temporary.path() + "/root"_L1;
    QVERIFY(put(root + u"/album/one.mp3"_s));
    const auto first = scan(root);
    QVERIFY(first);
    QCOMPARE(toJson(*scan(root, first)).value(u"albums"_s), toJson(*first).value(u"albums"_s));
    QVERIFY(QFile::rename(root + u"/album/one.mp3"_s, root + u"/album/two.mp3"_s));
    const auto renamed = scan(root, first);
    QCOMPARE(renamed->albums.first().id, first->albums.first().id);
    QVERIFY(renamed->albums.first().tracks.first().id != first->albums.first().tracks.first().id);
    QVERIFY(QDir().rename(root, root + u"-offline"_s));
    const auto offline = scan(root, renamed);
    QCOMPARE(offline->albums.size(), 1);
    QVERIFY(offline->albums.first().offline);
    QCOMPARE(offline->roots.first().status, u"offline"_s);
    QCOMPARE(offline->albums.first().tracks.first().id, renamed->albums.first().tracks.first().id);
    QVERIFY(QDir().rename(root + u"-offline"_s, root));
    const auto online = scan(root, offline);
    QVERIFY(!online->albums.first().offline);
    QVERIFY(QFile::remove(root + u"/album/two.mp3"_s));
    QVERIFY(scan(root, online)->albums.isEmpty());
    QVERIFY(put(root + u"/album/new.mp3"_s));
    const auto beforeMove = scan(root);
    QVERIFY(QDir().rename(root + u"/album"_s, root + u"/renamed"_s));
    const auto afterMove = scan(root, beforeMove);
    QVERIFY(afterMove->albums.first().id != beforeMove->albums.first().id);
    QVERIFY(afterMove->albums.first().tracks.first().id
            != beforeMove->albums.first().tracks.first().id);
    const auto removed = DirectoryScanner::scan({ }, online, { }).snapshot;
    QVERIFY(removed->albums.isEmpty());
    QVERIFY(removed->roots.isEmpty());
}
void DirectoryScannerTest::partialFailure()
{
#ifdef Q_OS_WIN
    QSKIP("POSIX directory permissions; Windows ACL denial is covered by the target handoff.");
#else
    QTemporaryDir temporary;
    const QString root = temporary.path();
    const QString blocked = root + "/z-blocked"_L1;
    QVERIFY(put(root + u"/a-readable/one.mp3"_s));
    QVERIFY(put(blocked + u"/two.mp3"_s));
    const auto previous = scan(root);
    QVERIFY(QFile::remove(root + u"/a-readable/one.mp3"_s));
    const auto oldPermissions = QFile::permissions(blocked);
    QVERIFY(QFile::setPermissions(blocked, { }));
    const auto denied = scan(root, previous);
    QVERIFY(QFile::setPermissions(blocked, oldPermissions));
    QVERIFY(denied);
    QCOMPARE(denied->roots.first().status, u"offline"_s);
    QCOMPARE(denied->albums.size(), 2);
    for (const auto &album : denied->albums)
        QVERIFY(album.offline);
#endif
}
void DirectoryScannerTest::cancellation()
{
    QTemporaryDir temporary;
    QVERIFY(put(temporary.path() + u"/album/file.mp3"_s));
    const auto token = std::make_shared<std::atomic_bool>(true);
    const auto result =
            DirectoryScanner::scan({ QUrl::fromLocalFile(temporary.path()) }, { }, token);
    QVERIFY(result.cancelled);
    QVERIFY(!result.snapshot);
    QVERIFY(result.error.isEmpty());
}
void DirectoryScannerTest::storeRoundTripAndResponsiveness()
{
    QTemporaryDir temporary;
    const QString root = temporary.path() + "/anonymous"_L1, cache = temporary.path() + "/data"_L1;
    for (int i = 0; i < 1500; ++i)
        QVERIFY(put(root + u"/album-%1/file.mp3"_s.arg(i)));
    OfflineMetadataProvider provider;
    LibrarySession session(&provider);
    LibraryStore store(cache);
    QObject::connect(&store, &LibraryStore::snapshotReady, &session, [&](Snapshot snapshot) {
        QCOMPARE(QThread::currentThread(), session.thread());
        session.setSnapshot(*snapshot);
    });
    QSignalSpy ready(&store, &LibraryStore::snapshotReady), saved(&store, &LibraryStore::saved);
    QSignalSpy resets(session.albums(), &QAbstractItemModel::modelReset);
    int ticks = 0, duringScan = 0;
    qint64 maximumGapNs = 0;
    QElapsedTimer tickTime;
    tickTime.start();
    QTimer heartbeat;
    heartbeat.setInterval(1);
    connect(&heartbeat, &QTimer::timeout, &store, [&] {
        maximumGapNs = qMax(maximumGapNs, tickTime.nsecsElapsed());
        tickTime.restart();
        ++ticks;
        duringScan += store.scanning();
    });
    heartbeat.start();
    QVERIFY(store.setRoots({ QUrl::fromLocalFile(root) }));
    QTRY_VERIFY_WITH_TIMEOUT(!store.busy(), LargeScanDeadlineMilliseconds);
    heartbeat.stop();
    QCOMPARE(ready.size(), 1);
    QCOMPARE(saved.size(), 1);
    QCOMPARE(resets.size(), 1);
    QCOMPARE(session.snapshot()->albums.size(), 1500);
    QVERIFY(duringScan > 0);
    qInfo() << "Anonymous scan GUI heartbeat ticks" << ticks << "during scan" << duringScan
            << "maximum event gap ns (includes GUI snapshot application)" << maximumGapNs;
    QVERIFY(store.error().isEmpty());
    QVERIFY(!QDir(cache).entryList({ u"*.tmp*"_s }, QDir::Files).size());
    LibraryStore loaded(cache);
    QSignalSpy loadedReady(&loaded, &LibraryStore::snapshotReady);
    loaded.load();
    QTRY_VERIFY_WITH_TIMEOUT(!loaded.busy(), AsyncDeadlineMilliseconds);
    QCOMPARE(loadedReady.size(), 1);
    QCOMPARE(toJson(*loaded.snapshot()),
             QJsonDocument::fromJson(yalahoo::test::readFile(cache + u"/library-index.json"_s))
                     .object());
    QCOMPARE(loaded.roots(), QList<QUrl>{ QUrl::fromLocalFile(root) });
    QCOMPARE(loaded.snapshot()->albums.size(), 1500);
}
void DirectoryScannerTest::latestRootAndCancelledResult()
{
    QTemporaryDir temporary;
    const QString a = temporary.path() + "/a"_L1, b = temporary.path() + "/b"_L1;
    QVERIFY(put(a + u"/old.mp3"_s));
    QVERIFY(put(b + u"/new.mp3"_s));
    const QString data = temporary.path() + "/data"_L1;
    LibraryStore store(data);
    QSignalSpy ready(&store, &LibraryStore::snapshotReady);
    QVERIFY(store.setRoots({ QUrl::fromLocalFile(a) }));
    for (int i = 0; i < 100; ++i)
        QVERIFY(store.setRoots({ QUrl::fromLocalFile(i == 99 ? b : a) }));
    QTRY_VERIFY_WITH_TIMEOUT(!store.busy(), AsyncDeadlineMilliseconds);
    QCOMPARE(ready.size(), 1);
    QCOMPARE(store.snapshot()->albums.first().title, u"new"_s);
    const auto bytes = yalahoo::test::readFile(data + "/library-index.json"_L1);
    ready.clear();
    // Cancel after the worker finishes but before its queued completion is delivered.
    QVERIFY(store.setRoots({ QUrl::fromLocalFile(a) }));
    store.cancel();
    QTRY_VERIFY_WITH_TIMEOUT(!store.busy(), AsyncDeadlineMilliseconds);
    QCOMPARE(ready.size(), 0);
    QCOMPARE(store.snapshot()->albums.first().title, u"new"_s);
    QCOMPARE(yalahoo::test::readFile(data + u"/library-index.json"_s), bytes);
    // Changing roots during cache loading must still reuse existing metadata.
    LibraryStore reloaded(data);
    reloaded.load();
    QVERIFY(reloaded.setRoots({ QUrl::fromLocalFile(b) }));
    QTRY_VERIFY_WITH_TIMEOUT(!reloaded.busy(), AsyncDeadlineMilliseconds);
    QCOMPARE(toJson(*reloaded.snapshot()).value(u"albums"_s),
             toJson(*store.snapshot()).value(u"albums"_s));
}
void DirectoryScannerTest::badCacheAndWriteFailure()
{
    QTemporaryDir temporary;
    const QString root = temporary.path() + "/music"_L1, data = temporary.path() + "/data"_L1;
    const QString cache = data + "/library-index.json"_L1;
    QVERIFY(put(root + u"/file.mp3"_s));
    for (const auto &bytes :
         { "{broken"_ba, "{\"nativeSchemaVersion\":999}"_ba,
           QByteArray(
                   "{\"nativeSchemaVersion\":1,\"albums\":[],\"genres\":[],\"roots\":[{\"path\":\"relative\"}]}"_ba) }) {
        QVERIFY(put(cache, bytes));
        LibraryStore store(data);
        QSignalSpy failed(&store, &LibraryStore::failed);
        // setRoots itself must protect an existing bad cache, even without explicit load().
        QVERIFY(store.setRoots({ QUrl::fromLocalFile(root) }));
        QTRY_VERIFY_WITH_TIMEOUT(!store.busy(), AsyncDeadlineMilliseconds);
        QVERIFY(failed.size() > 0);
        QCOMPARE(store.snapshot()->albums.size(), 1);
        QCOMPARE(yalahoo::test::readFile(cache), bytes);
    }
    QVERIFY(QFile::remove(cache));
    QVERIFY(QDir().mkpath(cache));
    LibraryStore failedRead(data);
    failedRead.load();
    QTRY_VERIFY_WITH_TIMEOUT(!failedRead.busy(), AsyncDeadlineMilliseconds);
    QVERIFY(!failedRead.error().isEmpty());
    // A source file in place of the data directory makes cache writes fail safely.
    const QString blockedData = temporary.path() + "/blocked"_L1;
    QVERIFY(put(blockedData, "unchanged"_ba));
    LibraryStore failedWrite(blockedData);
    QVERIFY(failedWrite.setRoots({ QUrl::fromLocalFile(root) }));
    QTRY_VERIFY_WITH_TIMEOUT(!failedWrite.busy(), AsyncDeadlineMilliseconds);
    QVERIFY(!failedWrite.error().isEmpty());
    QCOMPARE(yalahoo::test::readFile(blockedData), "unchanged"_ba);
}
void DirectoryScannerTest::atomicSaveFailure()
{
    QTemporaryDir temporary;
    const QString root = temporary.path() + "/music"_L1, data = temporary.path() + "/data"_L1;
    const QString cache = data + "/library-index.json"_L1;
    QVERIFY(put(root + u"/first.mp3"_s));
    LibraryStore store(data);
    QVERIFY(store.setRoots({ QUrl::fromLocalFile(root) }));
    QTRY_VERIFY_WITH_TIMEOUT(!store.busy(), AsyncDeadlineMilliseconds);
    QVERIFY(store.error().isEmpty());
    const auto original = yalahoo::test::readFile(cache);
    QVERIFY(!original.isEmpty());
    QVERIFY(put(root + u"/second.mp3"_s));
#ifdef Q_OS_WIN
    const QString protectedPath = cache;
    const auto deniedPermissions = QFileDevice::ReadOwner;
#else
    const QString protectedPath = data;
    const auto deniedPermissions = QFileDevice::ReadOwner | QFileDevice::ExeOwner;
#endif
    const auto permissions = QFile::permissions(protectedPath);
    QVERIFY(QFile::setPermissions(protectedPath, deniedPermissions));
    store.rescan();
    // Restore access before assertions/cleanup even if an operation fails.
    QElapsedTimer deadline;
    deadline.start();
    while (store.busy() && deadline.elapsed() < 10000)
        QTest::qWait(EventPollMilliseconds);
    const bool idle = !store.busy(), failed = !store.error().isEmpty();
    QVERIFY(QFile::setPermissions(protectedPath, permissions));
    QVERIFY(idle);
    QVERIFY(failed);
    QCOMPARE(store.snapshot()->albums.size(), 2);
    QCOMPARE(yalahoo::test::readFile(cache), original);
    QVERIFY(QDir(data).entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot)
            == QStringList{ u"library-index.json"_s });
}
void DirectoryScannerTest::destruction()
{
    QTemporaryDir temporary;
    const QString root = temporary.path() + "/music"_L1, data = temporary.path() + "/data"_L1;
    QVERIFY(put(root + u"/file.mp3"_s));
    {
        LibraryStore store(data);
        QVERIFY(store.setRoots({ QUrl::fromLocalFile(root) }));
    }
    QVERIFY(!QFile::exists(data + u"/library-index.json"_s));
    {
        LibraryStore store(data);
        QSignalSpy ready(&store, &LibraryStore::snapshotReady);
        QVERIFY(store.setRoots({ QUrl::fromLocalFile(root) }));
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, AsyncDeadlineMilliseconds);
    }
    LibraryStore loaded(data);
    loaded.load();
    QTRY_VERIFY_WITH_TIMEOUT(!loaded.busy(), AsyncDeadlineMilliseconds);
    QCOMPARE(loaded.snapshot()->albums.size(), 1);
}
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().size() == 3 && app.arguments()[1] == "--fingerprint"_L1) {
        const auto path = app.arguments()[2];
        QString error;
        const auto stamp = DirectoryScanner::fingerprint(path, &error);
        if (!error.isEmpty())
            return 1;
        const auto json =
                QJsonDocument(
                        QJsonObject{ { u"fingerprint"_s, stamp },
                                     { u"id"_s, DirectoryScanner::stableId(u"track"_s, path) } })
                        .toJson(QJsonDocument::Compact);
        std::puts(json.constData());
        return 0;
    }
    DirectoryScannerTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "DirectoryScannerTest.moc"
