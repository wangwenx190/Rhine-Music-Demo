#include "AppIdentity.h"

#include <QCoreApplication>
#include <QGuiApplication>

void yalahoo::initializeIdentity()
{
    QCoreApplication::setOrganizationName(QStringLiteral("yalahoo"));
    QCoreApplication::setApplicationName(QStringLiteral("yalahoo"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("yalahOO!!!"));
}
