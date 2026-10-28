#include "SingleInstance.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QCryptographicHash>
#include <QTimer>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <cstdio>

using namespace Qt::StringLiterals;
using namespace yalahoo;
namespace {
void launch(QProcess &process, const QString &root, const QString &mode = u"probe"_s)
{
    process.start(QCoreApplication::applicationFilePath(), { u"--instance-peer"_s, root, mode });
}
}
class SingleInstanceTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SingleInstanceTest)
public:
    SingleInstanceTest() = default;
private Q_SLOTS:
    void acknowledgementFraming_data()
    {
        QTest::addColumn<QByteArray>("tail");
        QTest::addColumn<int>("expected");
        QTest::newRow("fragmented-valid") << "k\n"_ba << int(SingleInstance::Result::Secondary);
        QTest::newRow("fragmented-invalid") << "x\n"_ba << int(SingleInstance::Result::Error);
        QTest::newRow("incomplete-disconnect")
                << QByteArray() << int(SingleInstance::Result::Error);
        QTest::newRow("missing-newline") << "k"_ba << int(SingleInstance::Result::Error);
    }
    void acknowledgementFraming()
    {
        QFETCH(QByteArray, tail);
        QFETCH(int, expected);
        constexpr int FragmentDelayMilliseconds = 40;
        QTemporaryDir directory;
        QLockFile lock(directory.filePath(u"instance.lock"_s));
        QVERIFY(lock.tryLock());
        const auto canonical = QDir(directory.path()).canonicalPath();
        const auto digest = QCryptographicHash::hash(canonical.toUtf8(), QCryptographicHash::Sha256)
                                    .toHex()
                                    .first(32);
        const auto endpoint = u"yalahoo-"_s + QString::fromUtf8(digest);
        QLocalServer server;
        server.setSocketOptions(QLocalServer::UserAccessOption);
        QVERIFY2(server.listen(endpoint), qPrintable(server.errorString()));
        connect(&server, &QLocalServer::newConnection, this, [&] {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QLocalSocket::readyRead, socket, [socket, tail] {
                if (!socket->canReadLine())
                    return;
                if (socket->readAll() != "activate\n"_ba)
                    return;
                socket->write("o"_ba);
                socket->flush();
                QTimer::singleShot(FragmentDelayMilliseconds, socket, [socket, tail] {
                    socket->write(tail);
                    socket->disconnectFromServer();
                });
            });
        });
        QProcess peer;
        launch(peer, directory.path());
        QVERIFY(peer.waitForStarted());
        QTRY_COMPARE(peer.state(), QProcess::NotRunning);
        QCOMPARE(peer.exitStatus(), QProcess::NormalExit);
        QCOMPARE(peer.exitCode(), expected);
    }
    void duplicateActivatesOwner()
    {
        QTemporaryDir directory;
        SingleInstance owner;
        QVERIFY2(owner.start(directory.path(), true) == SingleInstance::Result::Primary,
                 qPrintable(owner.error()));
        QSignalSpy activation(&owner, &SingleInstance::activationRequested);
        QProcess peer;
        launch(peer, directory.path());
        QVERIFY(peer.waitForStarted());
        QTRY_COMPARE(peer.state(), QProcess::NotRunning);
        QCOMPARE(peer.exitStatus(), QProcess::NormalExit);
        QCOMPARE(peer.exitCode(), int(SingleInstance::Result::Secondary));
        QCOMPARE(activation.size(), 1);
        QVERIFY(owner.error().isEmpty());
    }
    void canonicalRootAndDisabledMode()
    {
        QTemporaryDir directory;
        const auto root = directory.path();
        SingleInstance owner;
        QVERIFY2(owner.start(root, true) == SingleInstance::Result::Primary,
                 qPrintable(owner.error()));
        QSignalSpy activation(&owner, &SingleInstance::activationRequested);
        QProcess peer;
        launch(peer, root + u"/../"_s + QFileInfo(root).fileName());
        QVERIFY(peer.waitForStarted());
        QTRY_COMPARE(peer.state(), QProcess::NotRunning);
        QCOMPARE(peer.exitCode(), int(SingleInstance::Result::Secondary));
        QCOMPARE(activation.size(), 1);
        SingleInstance disabled;
        QCOMPARE(disabled.start(root, false), SingleInstance::Result::Primary);
        SingleInstance isolated;
        QCOMPARE(isolated.start(directory.filePath(u"other"_s), true),
                 SingleInstance::Result::Primary);
    }
    void releaseAndCrashRecovery()
    {
        QTemporaryDir directory;
        {
            SingleInstance owner;
            QVERIFY2(owner.start(directory.path(), true) == SingleInstance::Result::Primary,
                     qPrintable(owner.error()));
        }
        QProcess owner;
        launch(owner, directory.path(), u"hold"_s);
        QVERIFY(owner.waitForStarted());
        QVERIFY(owner.waitForReadyRead());
        QCOMPARE(owner.readAllStandardOutput(), "ready\n"_ba);
        owner.kill();
        QVERIFY(owner.waitForFinished());
        SingleInstance replacement;
        QCOMPARE(replacement.start(directory.path(), true), SingleInstance::Result::Primary);
    }
    void unreachableOwnerDoesNotBecomePrimary()
    {
        QTemporaryDir directory;
        QLockFile owner(directory.filePath(u"instance.lock"_s));
        QVERIFY(owner.tryLock());
        QProcess peer;
        launch(peer, directory.path());
        QVERIFY(peer.waitForStarted());
        QTRY_COMPARE(peer.state(), QProcess::NotRunning);
        QCOMPARE(peer.exitStatus(), QProcess::NormalExit);
        QCOMPARE(peer.exitCode(), int(SingleInstance::Result::Error));
        QVERIFY(owner.isLocked());
    }
    void failuresDoNotBecomePrimary()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(u"file"_s);
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        SingleInstance invalid;
        QCOMPARE(invalid.start(path, true), SingleInstance::Result::Error);
        QVERIFY(!invalid.error().isEmpty());
    }
};
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const auto arguments = app.arguments();
    if (arguments.size() == 4 && arguments[1] == "--instance-peer"_L1) {
        SingleInstance instance;
        const auto result = instance.start(arguments[2], true);
        if (result != SingleInstance::Result::Primary || arguments[3] != "hold"_L1)
            return int(result);
        std::fputs("ready\n", stdout);
        std::fflush(stdout);
        return app.exec();
    }
    SingleInstanceTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "SingleInstanceTest.moc"
