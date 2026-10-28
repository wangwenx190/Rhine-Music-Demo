#include "AppIdentity.h"

#include <QCoreApplication>
#include <QGuiApplication>

using namespace Qt::StringLiterals;

void yalahoo::initializeIdentity()
{
    QCoreApplication::setOrganizationName(u"yalahoo"_s);
    QCoreApplication::setApplicationName(u"yalahoo"_s);
    QGuiApplication::setApplicationDisplayName(u"yalahOO!!!"_s);
}
