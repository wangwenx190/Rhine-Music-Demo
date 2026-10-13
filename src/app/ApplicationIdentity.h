#pragma once
#include <QGuiApplication>
#include <QObject>
#include <QtQml/qqmlregistration.h>

namespace yalahoo {
class ApplicationIdentity : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ApplicationIdentity)
    QML_SINGLETON
    Q_PROPERTY(QString displayName READ displayName CONSTANT)
public:
    using QObject::QObject;
    QString displayName() const
    {
        return QGuiApplication::applicationDisplayName();
    }
};
}
