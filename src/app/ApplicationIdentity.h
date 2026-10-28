// Establishes product names before Qt resolves settings and application paths. Literal
// identifiers keep branding separate from persistent technical identity.

#pragma once
#include <QGuiApplication>
#include <QObject>
#include <QtQml/qqmlregistration.h>

namespace yalahoo {
class ApplicationIdentity final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ApplicationIdentity)
    QML_NAMED_ELEMENT(ApplicationIdentity)
    QML_SINGLETON
    Q_PROPERTY(QString displayName READ displayName CONSTANT FINAL)
public:
    ApplicationIdentity() = default;
    using QObject::QObject;
    QString displayName() const
    {
        return QGuiApplication::applicationDisplayName();
    }
};
}
