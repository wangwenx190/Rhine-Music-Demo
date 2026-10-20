// Registers bundled fonts and resolves their physical faces. Registration is transactional so
// missing assets cannot leave a partially usable catalog.

#pragma once
#include <QFont>
#include <QObject>
#include <QtQml/qqmlregistration.h>
namespace yalahoo {
namespace assetFontWeight {
// These are CSS numeric weights from the bundled font manifest.
inline constexpr auto Light = 300;
inline constexpr auto Regular = 400;
inline constexpr auto DemiBold = 600;
inline constexpr auto Bold = 700;
}

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
        return assetFont(assetFontWeight::Light);
    }
    QFont regular() const
    {
        return assetFont(assetFontWeight::Regular);
    }
    QFont demibold() const
    {
        return assetFont(assetFontWeight::DemiBold);
    }
    QFont bold() const
    {
        return assetFont(assetFontWeight::Bold);
    }
};
}
