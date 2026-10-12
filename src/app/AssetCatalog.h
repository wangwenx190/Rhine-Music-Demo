#pragma once
#include <QFont>
#include <QObject>
#include <QtQml/qqmlregistration.h>
namespace yalahoo {
// Registers the four embedded faces before constructing the QML engine.
// Does not read the file system or mutate user data.
bool initializeAssetFonts();
QFont assetFont(int cssWeight);
class AssetFonts : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(MusicFonts)
    QML_SINGLETON
    Q_PROPERTY(QFont light READ light CONSTANT)
    Q_PROPERTY(QFont regular READ regular CONSTANT)
    Q_PROPERTY(QFont demibold READ demibold CONSTANT)
    Q_PROPERTY(QFont bold READ bold CONSTANT)
public:
    using QObject::QObject;
    QFont light() const
    {
        return assetFont(300);
    }
    QFont regular() const
    {
        return assetFont(400);
    }
    QFont demibold() const
    {
        return assetFont(600);
    }
    QFont bold() const
    {
        return assetFont(700);
    }
};
}
