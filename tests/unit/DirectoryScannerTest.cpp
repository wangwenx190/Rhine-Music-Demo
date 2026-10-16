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

using namespace yalahoo;
namespace {
bool put(const QString &path, const QByteArray &bytes = "anonymous audio fixture")
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
QByteArray get(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return { };
    return file.readAll();
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
class DirectoryScannerTest : public QObject
{
    Q_OBJECT
private slots:
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
    const QString music = temporary.path() + QStringLiteral("/音乐 目录 #1");
    QVERIFY(QDir().mkpath(music));
    LibraryStore store(temporary.path() + "/data");
    QVERIFY(!store.setRootPaths("relative/path"));
    QVERIFY(store.setRootPaths("\n" + music + "\n" + music + "\n"));
    QCOMPARE(store.rootPaths(), QStringList{ music });
    QCOMPARE(store.localPath(QUrl::fromLocalFile(music)), music);
    QTRY_VERIFY_WITH_TIMEOUT(!store.busy(), 10000);
    QCOMPARE(store.roots().size(), 1);
    QVERIFY(store.setRootPaths(""));
    QTRY_VERIFY_WITH_TIMEOUT(!store.busy(), 10000);
    QVERIFY(store.rootPaths().isEmpty());
}
void DirectoryScannerTest::rootValidation()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto root = temporary.path();
    QList<QUrl> output{ QUrl::fromLocalFile(root) };
    QString error;
    const auto nested = QUrl::fromLocalFile(root + "/child");
    QVERIFY(DirectoryScanner::normalizeRoots(
            { nested, QUrl::fromLocalFile(root + "/./"), QUrl::fromLocalFile(root) }, output,
            error));
    QCOMPARE(output, QList<QUrl>{ QUrl::fromLocalFile(root) });
    for (const auto &invalid : { QUrl("https://example.invalid/music"), QUrl("relative"),
                                 QUrl::fromLocalFile(QString("/bad") + QChar::Null) }) {
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
    const QString root = temporary.path() + QStringLiteral("/音乐 空格 # %");
    const QString album = root + "/专辑";
    const QStringList extensions{ "ape", "wv",  "wma", "flac", "wav",  "m4a", "mp4", "alac",
                                  "dsf", "dff", "mp3", "aac",  "aiff", "aif", "ogg", "opus" };
    for (const auto &ext : extensions)
        QVERIFY(put(album + "/曲目." + ext.toUpper()));
    QVERIFY(put(root + "/直属1.mp3"));
    QVERIFY(put(root + "/直属2.flac"));
    QVERIFY(put(root + "/多 CD/CD1/1.wav"));
    QVERIFY(put(root + "/多 CD/CD2/2.wav"));
    QVERIFY(put(root + "/.忽略/隐藏.mp3"));
    QVERIFY(put(album + "/ignored.txt"));
    QVERIFY(put(album + "/.可索引.mp3"));
    const auto result = scan(root);
    QVERIFY(result);
    QCOMPARE(result->albums.size(), 5);
    QCOMPARE(atFolder(result, album)->tracks.size(), 17);
    QCOMPARE(result->roots.first().status, "online");
    int singles = 0;
    for (const auto &entry : result->albums) {
        const bool single = entry.fields.value("_singleFile").toBool();
        singles += single;
        const QString identity =
                single ? entry.tracks.first().sourceUrl.toLocalFile() : entry.folder.toLocalFile();
        const auto hash = QCryptographicHash::hash(QDir::toNativeSeparators(identity).toUtf8(),
                                                   QCryptographicHash::Sha256)
                                  .toHex()
                                  .first(24);
        QCOMPARE(entry.id, "album-" + QString::fromLatin1(hash));
        for (const auto &track : entry.tracks) {
            QCOMPARE(track.albumId, entry.id);
            QCOMPARE(track.mediaSupport, MediaSupport::Unknown);
            QCOMPARE(track.sourceUrl.toLocalFile(), root + '/' + track.relativePath);
            QVERIFY(!track.sourceUrl.hasQuery());
            QVERIFY(!track.sourceUrl.hasFragment());
        }
    }
    QCOMPARE(singles, 2);
}
void DirectoryScannerTest::symlinks()
{
    QTemporaryDir temporary;
    const QString root = temporary.path() + "/root";
    QVERIFY(put(root + "/album/file.mp3"));
    QVERIFY(put(temporary.path() + "/outside/file.mp3"));
    const auto native = [](const QString &path) {
#ifdef Q_OS_WIN
        return std::filesystem::path(path.toStdWString());
#else
        return std::filesystem::path(path.toUtf8().toStdString());
#endif
    };
    std::error_code error;
    std::filesystem::create_directory_symlink(native(root), native(root + "/album/cycle"), error);
#ifdef Q_OS_WIN
    if (error)
        QSKIP("Native symlink creation unavailable; Windows symlink/junction checks remain R21-P.");
#endif
    QVERIFY2(!error, error.message().c_str());
    std::filesystem::create_directory_symlink(native(root + "/album"), native(root + "/duplicate"),
                                              error);
    QVERIFY2(!error, error.message().c_str());
    std::filesystem::create_directory_symlink(native(temporary.path() + "/outside"),
                                              native(root + "/external"), error);
    QVERIFY2(!error, error.message().c_str());
    std::filesystem::create_symlink(native(root + "/album/file.mp3"), native(root + "/alias.mp3"),
                                    error);
    QVERIFY2(!error, error.message().c_str());
    const auto result = scan(root);
    QCOMPARE(result->albums.size(), 1);
    QCOMPARE(result->albums.first().tracks.size(), 1);
    QCOMPARE(scan(root + "/duplicate")->roots.first().status, "offline");
}
void DirectoryScannerTest::fingerprintAndReuse()
{
    QTemporaryDir temporary;
    const QString path = temporary.path() + "/album/1-02 歌曲.FLAC";
    QVERIFY(put(path, "abc"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadWrite));
    QVERIFY(file.setFileTime(QDateTime::fromMSecsSinceEpoch(1700000000123, QTimeZone::UTC),
                             QFileDevice::FileModificationTime));
    file.close();
    QCOMPARE(DirectoryScanner::fingerprint(path), "3:1700000000123");
    const auto originalHash = QCryptographicHash::hash(get(path), QCryptographicHash::Sha256);
    auto first = scan(temporary.path());
    QVERIFY(first);
    auto previous = *first;
    previous.fields.insert("futureLibraryField", 17);
    previous.scan.insert("futureScanField", "retained");
    auto &album = previous.albums.first();
    album.title = "metadata album";
    album.localNote = "local note";
    album.fields.insert("futureAlbumField", 42);
    auto &track = album.tracks.first();
    QCOMPARE(track.discNumber, std::optional<int>(1));
    QCOMPARE(track.trackNumber, std::optional<int>(2));
    track.title = "metadata title";
    track.fields.insert("_metadataVersion", 4);
    track.fields.insert("futureTrackField", "retained");
    track.mediaSupport = MediaSupport::Supported;
    auto second = scan(temporary.path(), std::make_shared<const LibrarySnapshot>(previous));
    QCOMPARE(second->albums.first().tracks.first().title, "metadata title");
    QCOMPARE(second->albums.first().localNote, "local note");
    QCOMPARE(second->fields.value("futureLibraryField").toInt(), 17);
    QCOMPARE(second->scan.value("futureScanField").toString(), "retained");
    QCOMPARE(second->genres.first().albumCount, std::optional<int>(1));
    QCOMPARE(second->albums.first().fields.value("futureAlbumField").toInt(), 42);
    QCOMPARE(QCryptographicHash::hash(get(path), QCryptographicHash::Sha256), originalHash);
    QVERIFY(put(path, "changed source fixture"));
    auto third = scan(temporary.path(), second);
    const auto &changed = third->albums.first().tracks.first();
    QCOMPARE(changed.id, track.id);
    QCOMPARE(changed.title, "1-02 歌曲");
    QCOMPARE(changed.mediaSupport, MediaSupport::Unknown);
    QVERIFY(!changed.fields.contains("_metadataVersion"));
    QCOMPARE(changed.fields.value("futureTrackField").toString(), "retained");
    QCOMPARE(second->albums.first().tracks.first().title, "metadata title");
}
void DirectoryScannerTest::rescanRenameDeleteOffline()
{
    QTemporaryDir temporary;
    const QString root = temporary.path() + "/root";
    QVERIFY(put(root + "/album/one.mp3"));
    const auto first = scan(root);
    QVERIFY(first);
    QCOMPARE(toJson(*scan(root, first)).value("albums"), toJson(*first).value("albums"));
    QVERIFY(QFile::rename(root + "/album/one.mp3", root + "/album/two.mp3"));
    const auto renamed = scan(root, first);
    QCOMPARE(renamed->albums.first().id, first->albums.first().id);
    QVERIFY(renamed->albums.first().tracks.first().id != first->albums.first().tracks.first().id);
    QVERIFY(QDir().rename(root, root + "-offline"));
    const auto offline = scan(root, renamed);
    QCOMPARE(offline->albums.size(), 1);
    QVERIFY(offline->albums.first().offline);
    QCOMPARE(offline->roots.first().status, "offline");
    QCOMPARE(offline->albums.first().tracks.first().id, renamed->albums.first().tracks.first().id);
    QVERIFY(QDir().rename(root + "-offline", root));
    const auto online = scan(root, offline);
    QVERIFY(!online->albums.first().offline);
    QVERIFY(QFile::remove(root + "/album/two.mp3"));
    QVERIFY(scan(root, online)->albums.isEmpty());
    QVERIFY(put(root + "/album/new.mp3"));
    const auto beforeMove = scan(root);
    QVERIFY(QDir().rename(root + "/album", root + "/renamed"));
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
    const QString blocked = root + "/z-blocked";
    QVERIFY(put(root + "/a-readable/one.mp3"));
    QVERIFY(put(blocked + "/two.mp3"));
    const auto previous = scan(root);
    QVERIFY(QFile::remove(root + "/a-readable/one.mp3"));
    const auto oldPermissions = QFile::permissions(blocked);
    QVERIFY(QFile::setPermissions(blocked, { }));
    const auto denied = scan(root, previous);
    QVERIFY(QFile::setPermissions(blocked, oldPermissions));
    QVERIFY(denied);
    QCOMPARE(denied->roots.first().status, "offline");
    QCOMPARE(denied->albums.size(), 2);
    for (const auto &album : denied->albums)
        QVERIFY(album.offline);
#endif
}
void DirectoryScannerTest::cancellation()
{
    QTemporaryDir temporary;
    QVERIFY(put(temporary.path() + "/album/file.mp3"));
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
    const QString root = temporary.path() + "/anonymous", cache = temporary.path() + "/data";
    for (int i = 0; i < 1500; ++i)
        QVERIFY(put(root + QString("/album-%1/file.mp3").arg(i)));
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
    QTRY_VERIFY_WITH_TIMEOUT(!store.busy(), 20000);
    heartbeat.stop();
    QCOMPARE(ready.size(), 1);
    QCOMPARE(saved.size(), 1);
    QCOMPARE(resets.size(), 1);
    QCOMPARE(session.snapshot()->albums.size(), 1500);
    QVERIFY(duringScan > 0);
    qInfo() << "Anonymous scan GUI heartbeat ticks" << ticks << "during scan" << duringScan
            << "maximum event gap ns (includes GUI snapshot application)" << maximumGapNs;
    QVERIFY(store.error().isEmpty());
    QVERIFY(!QDir(cache).entryList({ "*.tmp*" }, QDir::Files).size());
    LibraryStore loaded(cache);
    QSignalSpy loadedReady(&loaded, &LibraryStore::snapshotReady);
    loaded.load();
    QTRY_VERIFY_WITH_TIMEOUT(!loaded.busy(), 10000);
    QCOMPARE(loadedReady.size(), 1);
    QCOMPARE(toJson(*loaded.snapshot()),
             QJsonDocument::fromJson(get(cache + "/library-index.json")).object());
    QCOMPARE(loaded.roots(), QList<QUrl>{ QUrl::fromLocalFile(root) });
    QCOMPARE(loaded.snapshot()->albums.size(), 1500);
}
void DirectoryScannerTest::latestRootAndCancelledResult()
{
    QTemporaryDir temporary;
    const QString a = temporary.path() + "/a", b = temporary.path() + "/b";
    QVERIFY(put(a + "/old.mp3"));
    QVERIFY(put(b + "/new.mp3"));
    const QString data = temporary.path() + "/data";
    LibraryStore store(data);
    QSignalSpy ready(&store, &LibraryStore::snapshotReady);
    QVERIFY(store.setRoots({ QUrl::fromLocalFile(a) }));
    for (int i = 0; i < 100; ++i)
        QVERIFY(store.setRoots({ QUrl::fromLocalFile(i == 99 ? b : a) }));
    QTRY_VERIFY_WITH_TIMEOUT(!store.busy(), 10000);
    QCOMPARE(ready.size(), 1);
    QCOMPARE(store.snapshot()->albums.first().title, "new");
    const auto bytes = get(data + "/library-index.json");
    ready.clear();
    // Cancel after the worker finishes but before its queued completion is delivered.
    QVERIFY(store.setRoots({ QUrl::fromLocalFile(a) }));
    store.cancel();
    QTRY_VERIFY_WITH_TIMEOUT(!store.busy(), 10000);
    QCOMPARE(ready.size(), 0);
    QCOMPARE(store.snapshot()->albums.first().title, "new");
    QCOMPARE(get(data + "/library-index.json"), bytes);
    // Changing roots during cache loading must still reuse existing metadata.
    LibraryStore reloaded(data);
    reloaded.load();
    QVERIFY(reloaded.setRoots({ QUrl::fromLocalFile(b) }));
    QTRY_VERIFY_WITH_TIMEOUT(!reloaded.busy(), 10000);
    QCOMPARE(toJson(*reloaded.snapshot()).value("albums"),
             toJson(*store.snapshot()).value("albums"));
}
void DirectoryScannerTest::badCacheAndWriteFailure()
{
    QTemporaryDir temporary;
    const QString root = temporary.path() + "/music", data = temporary.path() + "/data";
    const QString cache = data + "/library-index.json";
    QVERIFY(put(root + "/file.mp3"));
    for (const auto &bytes :
         { QByteArray("{broken"), QByteArray("{\"nativeSchemaVersion\":999}"),
           QByteArray(
                   "{\"nativeSchemaVersion\":1,\"albums\":[],\"genres\":[],\"roots\":[{\"path\":\"relative\"}]}") }) {
        QVERIFY(put(cache, bytes));
        LibraryStore store(data);
        QSignalSpy failed(&store, &LibraryStore::failed);
        // setRoots itself must protect an existing bad cache, even without explicit load().
        QVERIFY(store.setRoots({ QUrl::fromLocalFile(root) }));
        QTRY_VERIFY_WITH_TIMEOUT(!store.busy(), 10000);
        QVERIFY(failed.size() > 0);
        QCOMPARE(store.snapshot()->albums.size(), 1);
        QCOMPARE(get(cache), bytes);
    }
    QVERIFY(QFile::remove(cache));
    QVERIFY(QDir().mkpath(cache));
    LibraryStore failedRead(data);
    failedRead.load();
    QTRY_VERIFY_WITH_TIMEOUT(!failedRead.busy(), 10000);
    QVERIFY(!failedRead.error().isEmpty());
    // A source file in place of the data directory makes cache writes fail safely.
    const QString blockedData = temporary.path() + "/blocked";
    QVERIFY(put(blockedData, "unchanged"));
    LibraryStore failedWrite(blockedData);
    QVERIFY(failedWrite.setRoots({ QUrl::fromLocalFile(root) }));
    QTRY_VERIFY_WITH_TIMEOUT(!failedWrite.busy(), 10000);
    QVERIFY(!failedWrite.error().isEmpty());
    QCOMPARE(get(blockedData), "unchanged");
}
void DirectoryScannerTest::atomicSaveFailure()
{
    QTemporaryDir temporary;
    const QString root = temporary.path() + "/music", data = temporary.path() + "/data";
    const QString cache = data + "/library-index.json";
    QVERIFY(put(root + "/first.mp3"));
    LibraryStore store(data);
    QVERIFY(store.setRoots({ QUrl::fromLocalFile(root) }));
    QTRY_VERIFY_WITH_TIMEOUT(!store.busy(), 10000);
    QVERIFY(store.error().isEmpty());
    const auto original = get(cache);
    QVERIFY(!original.isEmpty());
    QVERIFY(put(root + "/second.mp3"));
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
        QTest::qWait(10);
    const bool idle = !store.busy(), failed = !store.error().isEmpty();
    QVERIFY(QFile::setPermissions(protectedPath, permissions));
    QVERIFY(idle);
    QVERIFY(failed);
    QCOMPARE(store.snapshot()->albums.size(), 2);
    QCOMPARE(get(cache), original);
    QVERIFY(QDir(data).entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot)
            == QStringList{ "library-index.json" });
}
void DirectoryScannerTest::destruction()
{
    QTemporaryDir temporary;
    const QString root = temporary.path() + "/music", data = temporary.path() + "/data";
    QVERIFY(put(root + "/file.mp3"));
    {
        LibraryStore store(data);
        QVERIFY(store.setRoots({ QUrl::fromLocalFile(root) }));
    }
    QVERIFY(!QFile::exists(data + "/library-index.json"));
    {
        LibraryStore store(data);
        QSignalSpy ready(&store, &LibraryStore::snapshotReady);
        QVERIFY(store.setRoots({ QUrl::fromLocalFile(root) }));
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 10000);
    }
    LibraryStore loaded(data);
    loaded.load();
    QTRY_VERIFY_WITH_TIMEOUT(!loaded.busy(), 10000);
    QCOMPARE(loaded.snapshot()->albums.size(), 1);
}
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().size() == 3 && app.arguments()[1] == QLatin1String("--fingerprint")) {
        const auto path = app.arguments()[2];
        QString error;
        const auto stamp = DirectoryScanner::fingerprint(path, &error);
        if (!error.isEmpty())
            return 1;
        const auto json =
                QJsonDocument(QJsonObject{ { "fingerprint", stamp },
                                           { "id", DirectoryScanner::stableId("track", path) } })
                        .toJson(QJsonDocument::Compact);
        std::puts(json.constData());
        return 0;
    }
    DirectoryScannerTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "DirectoryScannerTest.moc"
