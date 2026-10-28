#include "Localization.h"
#include "MusicTypes.h"
#include <QJsonDocument>

#include <QCoreApplication>
#include <QFile>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QtTest>
#include <memory>

using namespace Qt::StringLiterals;

class LocalizationTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(LocalizationTest)
public:
    LocalizationTest() = default;

private Q_SLOTS:
    void englishSourceFallback()
    {
        const yalahoo::Localization localization(QLocale(QLocale::French, QLocale::France));
        QVERIFY(localization.loaded());
        QCOMPARE(QCoreApplication::translate("Yalahoo", "System default"), u"System default"_s);
    }
    void englishPluralForms()
    {
        const yalahoo::Localization localization(QLocale(QLocale::English, QLocale::UnitedStates));
        QVERIFY(localization.loaded());
        QCOMPARE(QCoreApplication::translate("Footer", " %n ALBUMS", nullptr, 1), u" 1 ALBUM"_s);
        QCOMPARE(QCoreApplication::translate("Footer", " %n ALBUMS", nullptr, 2), u" 2 ALBUMS"_s);
        QCOMPARE(QCoreApplication::translate("BrowseInfo", "%n TRACKS", nullptr, 1), u"1 TRACK"_s);
        QCOMPARE(QCoreApplication::translate("Overview", "%n albums", nullptr, 2), u"2 albums"_s);
    }
    void embeddedChineseCppAndQml()
    {
        const yalahoo::Localization localization(QLocale(QLocale::Chinese, QLocale::China));
        QVERIFY(localization.loaded());
        QVERIFY(QFile::exists(u":/i18n/yalahoo_zh_CN.qm"_s));
        QCOMPARE(QCoreApplication::translate("Yalahoo", "System default"),
                 u"\u7cfb\u7edf\u9ed8\u8ba4"_s);
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQml
            QtObject {
                readonly property string label: qsTranslate("PlaybackPanel", "Play")
                readonly property string identifier: "filled"
                readonly property string count: qsTranslate("Footer", " %n ALBUMS", "", 1)
            }
        )"_ba,
                          QUrl(u"qrc:/localization-test.qml"_s));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        const std::unique_ptr<QObject> object(component.create());
        QVERIFY(object);
        QCOMPARE(object->property("label").toString(), u"\u64ad\u653e"_s);
        QCOMPARE(object->property("identifier").toString(), u"filled"_s);
        QCOMPARE(object->property("count").toString(), u" 1 \u5f20\u4e13\u8f91"_s);
    }
    void libraryErrors_data()
    {
        QTest::addColumn<QJsonObject>("input");
        QTest::addColumn<QString>("english");
        QTest::addColumn<QString>("chinese");
        QTest::newRow("parser-error-1")
                << QJsonDocument::fromJson(R"({})"_ba).object()
                << u"Library requires albums and genres arrays"_s
                << u"\u97f3\u4e50\u5e93\u5fc5\u987b\u5305\u542b\u4e13\u8f91\u548c\u6d41\u6d3e\u6570\u7ec4\u3002"_s;
        QTest::newRow("parser-error-2")
                << QJsonDocument::fromJson(R"({"albums":[],"genres":[],"roots":1})"_ba).object()
                << u"Library roots must be an array"_s
                << u"\u97f3\u4e50\u5e93\u6839\u76ee\u5f55\u5fc5\u987b\u4e3a\u6570\u7ec4\u3002"_s;
        QTest::newRow("parser-error-3")
                << QJsonDocument::fromJson(R"({"albums":[],"genres":[],"onlineEnabled":"true"})"_ba)
                           .object()
                << u"Invalid library metadata types"_s
                << u"\u97f3\u4e50\u5e93\u5143\u6570\u636e\u7c7b\u578b\u65e0\u6548\u3002"_s;
        QTest::newRow("parser-error-4")
                << QJsonDocument::fromJson(R"({"albums":[],"genres":[1]})"_ba).object()
                << u"Invalid genre metadata types"_s
                << u"\u6d41\u6d3e\u5143\u6570\u636e\u7c7b\u578b\u65e0\u6548\u3002"_s;
        QTest::newRow("parser-error-5")
                << QJsonDocument::fromJson(R"({"albums":[],"genres":[{"id":""}]})"_ba).object()
                << u"Invalid or duplicate genre ID"_s
                << u"\u6d41\u6d3e\u6807\u8bc6\u65e0\u6548\u6216\u91cd\u590d\u3002"_s;
        QTest::newRow("parser-error-6")
                << QJsonDocument::fromJson(R"({"albums":[1],"genres":[]})"_ba).object()
                << u"Invalid album metadata types"_s
                << u"\u4e13\u8f91\u5143\u6570\u636e\u7c7b\u578b\u65e0\u6548\u3002"_s;
        QTest::newRow("parser-error-7")
                << QJsonDocument::fromJson(
                           R"({"albums":[{"id":"album","tracks":1}],"genres":[]})"_ba)
                           .object()
                << u"Album tracks/producers must be arrays"_s
                << u"\u4e13\u8f91\u66f2\u76ee\u548c\u5236\u4f5c\u4eba\u5458\u5fc5\u987b\u4e3a\u6570\u7ec4\u3002"_s;
        QTest::newRow("parser-error-8")
                << QJsonDocument::fromJson(R"({"albums":[{"id":"","tracks":[]}],"genres":[]})"_ba)
                           .object()
                << u"Invalid or duplicate album ID"_s
                << u"\u4e13\u8f91\u6807\u8bc6\u65e0\u6548\u6216\u91cd\u590d\u3002"_s;
        QTest::newRow("parser-error-9")
                << QJsonDocument::fromJson(
                           R"({"albums":[{"id":"album","tracks":[1]}],"genres":[]})"_ba)
                           .object()
                << u"Invalid track metadata types"_s
                << u"\u66f2\u76ee\u5143\u6570\u636e\u7c7b\u578b\u65e0\u6548\u3002"_s;
        QTest::newRow("parser-error-10")
                << QJsonDocument::fromJson(
                           R"({"albums":[{"id":"album","tracks":[{"id":"track","albumId":"other"}]}],"genres":[]})"_ba)
                           .object()
                << u"Invalid, duplicate or mismatched track ID"_s
                << u"\u66f2\u76ee\u6807\u8bc6\u65e0\u6548\u3001\u91cd\u590d\u6216\u4e0e\u4e13\u8f91\u4e0d\u5339\u914d\u3002"_s;
        QTest::newRow("parser-error-11")
                << QJsonDocument::fromJson(
                           R"({"albums":[{"id":"album","tracks":[],"producers":[1]}],"genres":[]})"_ba)
                           .object()
                << u"Invalid producer metadata types"_s
                << u"\u5236\u4f5c\u4eba\u5458\u5143\u6570\u636e\u7c7b\u578b\u65e0\u6548\u3002"_s;
        QTest::newRow("parser-error-12")
                << QJsonDocument::fromJson(R"({"albums":[],"genres":[],"roots":[1]})"_ba).object()
                << u"Invalid root metadata types"_s
                << u"\u6839\u76ee\u5f55\u5143\u6570\u636e\u7c7b\u578b\u65e0\u6548\u3002"_s;
    }
    void libraryErrors()
    {
        QFETCH(QJsonObject, input);
        QFETCH(QString, english);
        QFETCH(QString, chinese);
        yalahoo::LibrarySnapshot snapshot;
        QString error;
        {
            const yalahoo::Localization localization(
                    QLocale(QLocale::English, QLocale::UnitedStates));
            QVERIFY(localization.loaded());
            QVERIFY(!yalahoo::readLibrary(input, snapshot, &error));
            QCOMPARE(error, english);
        }
        {
            const yalahoo::Localization localization(QLocale(QLocale::Chinese, QLocale::China));
            QVERIFY(localization.loaded());
            QVERIFY(!yalahoo::readLibrary(input, snapshot, &error));
            QCOMPARE(error, chinese);
        }
    }
    void translatorLifetime()
    {
        const auto original = QCoreApplication::translate("Yalahoo", "System default");
        {
            const yalahoo::Localization localization(QLocale(QLocale::Chinese, QLocale::China));
            QVERIFY(localization.loaded());
        }
        QCOMPARE(QCoreApplication::translate("Yalahoo", "System default"), original);
    }
};

QTEST_GUILESS_MAIN(LocalizationTest)
#include "LocalizationTest.moc"
