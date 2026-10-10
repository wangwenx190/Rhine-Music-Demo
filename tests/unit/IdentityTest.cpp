#include "AppIdentity.h"
#include <QGuiApplication>
#include <QStandardPaths>
#include <QtTest>

class IdentityTest : public QObject {
    Q_OBJECT
private slots:
    void stableIdentity()
    {
        yalahoo::initializeIdentity();
        QCOMPARE(QCoreApplication::organizationName(), QStringLiteral("yalahoo"));
        QCOMPARE(QCoreApplication::applicationName(), QStringLiteral("yalahoo"));
        QCOMPARE(QGuiApplication::applicationDisplayName(), QStringLiteral("yalahOO!!!"));
        const QString before = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QGuiApplication::setApplicationDisplayName(QStringLiteral("Future display name"));
        QCOMPARE(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation), before);
        QVERIFY(before.contains(QStringLiteral("yalahoo")));
        QVERIFY(!before.contains(QLatin1Char('!')));
    }
};
QTEST_MAIN(IdentityTest)
#include "IdentityTest.moc"
