#include "JsonFile.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using namespace Qt::StringLiterals;

class JsonFileTest final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void replacesCompleteDocument()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath(u"report.json"_s);
        QVERIFY(yalahoo::writeJsonFile(path, QJsonDocument(QJsonObject{ { u"passed"_s, false } })));
        const QJsonArray expected{ true, u"replacement"_s };
        QVERIFY(yalahoo::writeJsonFile(path, QJsonDocument(expected)));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(file.readAll()).array(), expected);
    }
    void rejectsInvalidDocumentAndUnwritableDestination()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath(u"report.json"_s);
        const QJsonDocument original(QJsonObject{ { u"passed"_s, false } });
        QVERIFY(yalahoo::writeJsonFile(path, original));
        QVERIFY(!yalahoo::writeJsonFile(path, QJsonDocument()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), original.toJson());
        QVERIFY(!yalahoo::writeJsonFile(directory.filePath(u"missing/report.json"_s), original));
        QVERIFY(!yalahoo::writeJsonFile(directory.path(), original));
    }
};
QTEST_GUILESS_MAIN(JsonFileTest)
#include "JsonFileTest.moc"
