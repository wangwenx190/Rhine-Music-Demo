#include "../support/FileFixture.h"
#include "Logging.h"
#include "AppIdentity.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
#include <thread>
#include <vector>

using namespace Qt::StringLiterals;
using namespace yalahoo;

namespace {
int emitLogs(const QString &directory, const QString &mode)
{
    LoggingSession::Options options;
    options.directory = directory;
    if (mode == "rotate"_L1) {
        options.segmentBytes = 512;
        options.archiveCount = 2;
    }
    const LoggingSession logging(options);
    if (mode == "rotate"_L1) {
        qCInfo(logApp).noquote() << QString(1500, QChar(0x4e2d));
    } else {
        qCInfo(logApp) << "info-marker" << u"\u4e2d\u6587 \U0001f3b5"_s;
        qCWarning(logLibrary) << "warning-marker" << "code=" << 37;
        qCCritical(logMedia) << "error-marker" << "source=" << u"qrc:/fixture.wav"_s;
        qCDebug(logRender) << "debug-hidden-marker";
        const QMessageLogContext context(__FILE__, __LINE__, Q_FUNC_INFO, "yalahoo.render");
        // Exercise fatal severity flushing without intentionally aborting the test process.
        const auto handler = qInstallMessageHandler(nullptr);
        qInstallMessageHandler(handler);
        handler(QtFatalMsg, context, u"fatal-marker"_s);
        std::vector<std::jthread> writers;
        for (int thread = 0; thread < 3; ++thread)
            writers.emplace_back([thread] {
                for (int item = 0; item < 8; ++item)
                    qCInfo(logLibrary) << "worker-marker" << thread << item;
            });
    }
    return 0;
}
}

class LoggingTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(LoggingTest)
public:
    LoggingTest() = default;

private:
private:
    QByteArray run(const QString &directory, const QString &mode)
    {
        QProcess process;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.remove(u"QT_MESSAGE_PATTERN"_s);
        environment.remove(u"QT_LOGGING_RULES"_s);
        process.setProcessEnvironment(environment);
        process.start(QCoreApplication::applicationFilePath(), { u"--emit"_s, directory, mode });
        if (!process.waitForFinished(10000) || process.exitStatus() != QProcess::NormalExit
            || process.exitCode() != 0)
            return { };
        return process.readAllStandardError();
    }
private Q_SLOTS:
    void formatConcurrencyAndPipe()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto output = run(directory.path(), u"normal"_s);
        QVERIFY(output.contains("info-marker"));
        QVERIFY(output.contains("[warning] [yalahoo.library]"));
        QVERIFY(output.contains("[critical] [yalahoo.media]"));
        QVERIFY(output.contains("[fatal] [yalahoo.render]"));
        QVERIFY(output.contains(u"\u4e2d\u6587 \U0001f3b5"_s.toUtf8()));
        QVERIFY(!output.contains("\033["));
        QVERIFY(!output.contains("debug-hidden-marker"));
        QCOMPARE(output.count("worker-marker"), 24);
        const auto files = QDir(directory.path()).entryList({ u"*.log"_s }, QDir::Files);
        QCOMPARE(files.size(), 1);
        QCOMPARE(yalahoo::test::readFile(QDir(directory.path()).filePath(files.first())), output);
    }
    void rotationRetentionAndActiveOwner()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QDir root(directory.path());
        const auto oldStem =
                u"yalahoo-20000101T000000000Z-1-00000000-0000-0000-0000-000000000001"_s;
        const auto activeStem =
                u"yalahoo-20000101T000000000Z-1-00000000-0000-0000-0000-000000000002"_s;
        const auto oldPath = root.filePath(oldStem + u".0.log"_s);
        const auto activePath = root.filePath(activeStem + u".0.log"_s);
        const auto unrelated = root.filePath(u"unrelated.log"_s);
        for (const auto &path : { oldPath, activePath, unrelated }) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write("preserved"), 9);
            QVERIFY(file.flush());
            QVERIFY(file.setFileTime(QDateTime::currentDateTimeUtc().addDays(-20),
                                     QFileDevice::FileModificationTime));
        }
        QLockFile owner(root.filePath(activeStem + u".lock"_s));
        QVERIFY(owner.tryLock());
        const auto output = run(directory.path(), u"rotate"_s);
        QVERIFY(!output.isEmpty());
        QVERIFY(!QFile::exists(oldPath));
        QVERIFY(QFile::exists(activePath));
        QCOMPARE(yalahoo::test::readFile(unrelated), QByteArray("preserved"));
        int segments = 0;
        for (const auto &file : root.entryInfoList({ u"yalahoo-*.log"_s }, QDir::Files)) {
            if (file.absoluteFilePath() == activePath)
                continue;
            ++segments;
            QVERIFY(file.size() <= 512);
            const auto bytes = yalahoo::test::readFile(file.absoluteFilePath());
            QVERIFY(!bytes.contains("\033["));
            QCOMPARE(QString::fromUtf8(bytes).toUtf8(), bytes);
        }
        QCOMPARE(segments, 3);
    }
    void fileFailureStillReachesConsole()
    {
        QTemporaryDir directory;
        const auto path = QDir(directory.path()).filePath(u"occupied"_s);
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const auto output = run(path, u"normal"_s);
        QVERIFY(output.contains("Cannot create log directory"));
        QVERIFY(output.contains("error-marker"));
        QVERIFY(output.contains("Logging stopped"));
        QVERIFY(!output.contains("\033["));
    }
};

int main(int argc, char **argv)
{
    initializeIdentity();
    QCoreApplication app(argc, argv);
    const auto arguments = app.arguments();
    if (arguments.size() == 4 && arguments[1] == "--emit"_L1)
        return emitLogs(arguments[2], arguments[3]);
    LoggingTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "LoggingTest.moc"
