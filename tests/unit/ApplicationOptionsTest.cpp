// Exercises startup validation without creating media, persistence workers or windows.
#include "VerificationOptions.h"
#include <QDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace yalahoo;

class ApplicationOptionsTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ApplicationOptionsTest)
public:
    ApplicationOptionsTest() = default;
private Q_SLOTS:
    void productParserKeepsTheSupportedStartupProtocol()
    {
        const auto product = parseApplicationOptions({ u"yalahoo"_s, u"--demo"_s });
        QVERIFY(product.options);
        QVERIFY(product.options->demonstration());
        QVERIFY(!product.options->persistent());
        const auto normal = parseApplicationOptions({ u"yalahoo"_s });
        QVERIFY(normal.options);
        QVERIFY(normal.options->persistent());
        const auto externalCommand = parseApplicationOptions({ u"yalahoo"_s, u"--verify-app"_s });
        QVERIFY(!externalCommand.options);
        QVERIFY(!externalCommand.error.isEmpty());
    }
    void invalidCommands_data()
    {
        QTest::addColumn<QStringList>("arguments");
        QTest::addColumn<bool>("review");
        const auto path = QDir::tempPath();
        QTest::newRow("import-before-invalid-scan")
                << QStringList{ u"--import-preferences"_s, path, u"--music-root"_s, u"relative"_s }
                << false;
        QTest::newRow("import-before-conflicting-commands")
                << QStringList{ u"--import-preferences"_s, path, u"--import-legacy"_s, path,
                                u"--rescan-library"_s }
                << false;
        QTest::newRow("verification-and-import")
                << QStringList{ u"--verify-app"_s, u"--import-preferences"_s, path } << false;
        QTest::newRow("verification-and-data")
                << QStringList{ u"--verify-app"_s, u"--data-dir"_s, path } << false;
        QTest::newRow("conflicting-verifications")
                << QStringList{ u"--verify-app"_s, u"--verify-detail"_s, path } << false;
        QTest::newRow("demo-and-verification")
                << QStringList{ u"--demo"_s, u"--verify-app"_s } << false;
        QTest::newRow("demo-and-scan")
                << QStringList{ u"--lighting-lab"_s, u"--rescan-library"_s } << false;
        QTest::newRow("empty-data-root") << QStringList{ u"--data-dir="_s } << false;
        QTest::newRow("empty-output") << QStringList{ u"--verify-detail="_s } << false;
        QTest::newRow("unexpected-positional") << QStringList{ u"unused"_s } << false;
        QTest::newRow("unknown-option") << QStringList{ u"--unknown"_s } << false;
        QTest::newRow("review-unavailable")
                << QStringList{ u"--verify-materials"_s, path, u"--material-report"_s, path }
                << false;
        QTest::newRow("review-report-missing")
                << QStringList{ u"--verify-materials"_s, path } << true;
        QTest::newRow("review-report-orphan")
                << QStringList{ u"--material-report"_s, path } << true;
        QTest::newRow("relative-import")
                << QStringList{ u"--import-legacy"_s, u"relative"_s } << false;
    }
    void invalidCommands()
    {
        QFETCH(QStringList, arguments);
        QFETCH(bool, review);
        arguments.prepend(u"yalahoo"_s);
        const auto result = parseVerificationOptions(arguments, review);
        QVERIFY(!result.options);
        QVERIFY(!result.error.isEmpty());
        QVERIFY(result.help.isEmpty());
    }
    void validModesAndNativePaths()
    {
        const auto first = QDir::tempPath() + u"/Music \u4e2d\u6587"_s;
        const auto second = QDir::tempPath() + u"/Second directory"_s;
        const auto result = parseVerificationOptions(
                { u"yalahoo"_s, u"--music-root"_s, first, u"--music-root"_s, second,
                  u"--import-preferences"_s, first, u"--data-dir"_s, second },
                false);
        QVERIFY2(result.options.has_value(), qPrintable(result.error));
        QVERIFY(result.options->persistent());
        QCOMPARE(result.options->product.musicRoots, (QStringList{ first, second }));
        QCOMPARE(result.options->product.preferencesImport, first);
        QCOMPARE(result.options->product.dataRoot, second);
        const auto demo = parseVerificationOptions({ u"yalahoo"_s, u"--lighting-lab"_s }, false);
        QVERIFY(demo.options);
        QVERIFY(demo.options->product.demonstration());
        QVERIFY(!demo.options->persistent());
        QCOMPARE(demo.options->product.session, SessionMode::LightingLab);
        const auto verify =
                parseVerificationOptions({ u"yalahoo"_s, u"--verify-detail"_s, second }, false);
        QVERIFY(verify.options);
        QVERIFY(verify.options->appVerification());
        QVERIFY(!verify.options->persistent());
        QCOMPARE(verify.options->verificationInput, second);
        const auto material = parseVerificationOptions(
                { u"yalahoo"_s, u"--verify-materials"_s, first, u"--material-report"_s, second },
                true);
        QVERIFY(material.options);
        QVERIFY(material.options->verificationRun());
        QVERIFY(!material.options->appVerification());
        QCOMPARE(material.options->materialReport, second);
    }
    void missingMaterialReportPreservesCommandExitProtocol()
    {
        const auto result = parseVerificationOptions(
                { u"yalahoo"_s, u"--verify-materials"_s, u"fixture.json"_s }, true);
        QVERIFY(!result.options);
        QCOMPARE(result.errorCode,
                 std::to_underlying(VerificationArgumentError::MaterialReportMissing));
    }
    void helpDoesNotStartASession()
    {
        const auto result = parseVerificationOptions({ u"yalahoo"_s, u"--help"_s }, false);
        QVERIFY(!result.options);
        QVERIFY(result.error.isEmpty());
        QVERIFY(result.help.contains(u"--music-root"_s));
        QVERIFY(!result.help.contains(u"--verify-materials"_s));
    }
};
QTEST_GUILESS_MAIN(ApplicationOptionsTest)
#include "ApplicationOptionsTest.moc"
