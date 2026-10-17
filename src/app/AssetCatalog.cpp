#include "Logging.h"
#include "AssetCatalog.h"
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QRawFont>

using namespace Qt::StringLiterals;

namespace {
QMap<int, QFont> faceFonts;
}
QFont yalahoo::assetFont(int cssWeight)
{
    return faceFonts.value(cssWeight, faceFonts.value(400));
}

bool yalahoo::initializeAssetFonts()
{
    static bool initialized = false;
    if (initialized)
        return true;
    QFile manifest(u":/assets/manifests/fonts.json"_s);
    if (!manifest.open(QIODevice::ReadOnly))
        return false;
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(manifest.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return false;
    const auto fonts = document.object()[u"fonts"_s].toArray();
    if (fonts.size() != 4)
        return false;
    QList<int> registered;
    QMap<int, QFont> preparedFaces;
    auto rollback = [&] {
        for (const int previous : registered)
            QFontDatabase::removeApplicationFont(previous);
        return false;
    };
    for (const auto &value : fonts) {
        const auto path = value.toObject()[u"path"_s].toString();
        const int id = QFontDatabase::addApplicationFont(":/assets/"_L1 + path);
        if (id < 0)
            return rollback();
        registered.append(id);
        const auto families = QFontDatabase::applicationFontFamilies(id);
        if (families.isEmpty())
            return rollback();
        const auto face = value.toObject();
        QFont font(face[u"family"_s].toString());
        font.setStyleName(face[u"styleName"_s].toString());
        font.setWeight(QFont::Weight(face[u"sfntWeight"_s].toInt()));
        const QRawFont reference(":/assets/"_L1 + path, 24, QFont::PreferNoHinting);
        if (!reference.isValid())
            return rollback();
        const char *table = reference.fontTable("CFF ").isEmpty() ? "glyf" : "CFF ";
        const auto outlines = reference.fontTable(table);
        if (outlines.isEmpty())
            return rollback();
        auto matches = [&](const QFont &candidate) {
            return QRawFont::fromFont(candidate).fontTable(table) == outlines;
        };
        if (!families.contains(font.family(), Qt::CaseInsensitive) || !matches(font)) {
            // Avoid querying an unregistered alias: Qt can scan every system family.
            // DirectWrite/CoreText/Fontconfig can expose different aliases for
            // the same SFNT names. Find the registered face by its actual
            // outline table; never silently accept a substituted system face.
            bool found = false;
            for (const auto &family : families) {
                for (const auto &style : QFontDatabase::styles(family)) {
                    const auto candidate =
                            QFontDatabase::font(family, style, QGuiApplication::font().pointSize());
                    if (matches(candidate)) {
                        font = candidate;
                        found = true;
                        break;
                    }
                }
                if (found)
                    break;
            }
            if (!found)
                return rollback();
        }
        font.setStyleStrategy(QFont::StyleStrategy(QFont::PreferAntialias | QFont::PreferQuality));
        font.setHintingPreference(QFont::PreferNoHinting);
        preparedFaces.insert(face[u"cssWeight"_s].toInt(), font);
    }
    if (preparedFaces.keys() != QList<int>{ 300, 400, 600, 700 })
        return rollback();
#ifdef Q_OS_MACOS
    // Chromium's source path uses AppleColorEmoji, not CoreText's wider private UI face.
    // Select it once through Qt's emoji policy so whole-line shaping and QML Text agree.
    const QString emojiFamily = u"Apple Color Emoji"_s;
    if (!QFontDatabase::families().contains(emojiFamily)) {
        qCWarning(yalahoo::logApp, "Required macOS source emoji font is unavailable");
        return rollback();
    }
    QFontDatabase::setApplicationEmojiFontFamilies({ emojiFamily });
#endif
    faceFonts = preparedFaces;
    QGuiApplication::setFont(assetFont(400));
    initialized = true;
    return true;
}
