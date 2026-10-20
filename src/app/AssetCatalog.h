#pragma once
#include <QFont>
#include <QObject>
#include <QtQml/qqmlregistration.h>
namespace yalahoo {
// Registers the four embedded faces before constructing the QML engine.
// Does not read the file system or mutate user data.
bool initializeAssetFonts();
QFont assetFont(int cssWeight);
class AssetFonts final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AssetFonts)
    QML_NAMED_ELEMENT(MusicFonts)
    QML_SINGLETON
    Q_PROPERTY(QFont light READ light CONSTANT FINAL)
    Q_PROPERTY(QFont regular READ regular CONSTANT FINAL)
    Q_PROPERTY(QFont demibold READ demibold CONSTANT FINAL)
    Q_PROPERTY(QFont bold READ bold CONSTANT FINAL)
public:
    AssetFonts() = default;
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
