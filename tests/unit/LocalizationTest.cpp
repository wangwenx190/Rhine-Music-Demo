#include "Localization.h"

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
