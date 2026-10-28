#include "AppIdentity.h"
#include <QGuiApplication>
#include <QStandardPaths>
#include <QtTest>

using namespace Qt::StringLiterals;

class IdentityTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(IdentityTest)
public:
    IdentityTest() = default;

private Q_SLOTS:
    void stableIdentity()
    {
        yalahoo::initializeIdentity();
        QCOMPARE(QCoreApplication::organizationName(), u"yalahoo"_s);
        QCOMPARE(QCoreApplication::applicationName(), u"yalahoo"_s);
        QCOMPARE(QGuiApplication::applicationDisplayName(), u"yalahOO!!!"_s);
        const QString before = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QGuiApplication::setApplicationDisplayName(u"Future display name"_s);
        QCOMPARE(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation), before);
        QVERIFY(before.contains(u"yalahoo"_s));
        QVERIFY(!before.contains('!'_L1));
    }
};
QTEST_MAIN(IdentityTest)
#include "IdentityTest.moc"
