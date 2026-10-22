#include "GeometryAudit.h"
#include "AssetCatalog.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFontDatabase>
#include <QGlyphRun>
#include <QImage>
#include <QJsonDocument>
#include <QMediaPlayer>
#include <QPainter>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QRawFont>
#include <QRegularExpression>
#include <QSvgRenderer>
#include <QTextLayout>
#include <QtEndian>
#include <QtTest>
#include <stdexcept>

namespace {
// Bounds for asynchronous completion and event delivery; fixture values remain independent.
constexpr int AsyncDeadlineMilliseconds = 10000;
}

using namespace Qt::StringLiterals;

using namespace yalahoo::assets;
class AssetTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AssetTest)
public:
    AssetTest() = default;

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(yalahoo::initializeAssetFonts());
    }
    void embeddedHashes()
    {
        const auto manifest =
                QJsonDocument::fromJson(readFile(u":/assets/manifests/assets.json"_s)).object();
        QCOMPARE(manifest[u"baselineCommit"_s].toString(),
                 u"57f4f845c20cce646a15dbf4008c2a162aaaf4e2"_s);
        const auto entries = manifest[u"entries"_s].toArray();
        QCOMPARE(entries.size(), 47);
        for (const auto &value : entries) {
            const auto entry = value.toObject();
            const auto bytes = readFile(entry[u"qrc"_s].toString());
            QCOMPARE(bytes.size(), entry[u"bytes"_s].toInteger());
            QCOMPARE(QString::fromUtf8(
                             QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()),
                     entry[u"sha256"_s].toString());
            QVERIFY(!entry[u"path"_s].toString().contains(u"typing-preview"_s));
            QVERIFY(!entry[u"path"_s].toString().contains(u"app-icon"_s));
        }
    }
    void geometry()
    {
        QVector<Batch> imported;
        for (int i = 0; i < 3; ++i)
            imported.append(
                    readQtMesh(readFile(u":/assets/models/meshes/meshes_%1__mesh.mesh"_s.arg(i))));
        const auto report = compareGeometry(
                readGlb(readFile(u":/assets/models/music-case-v040.glb"_s)), imported);
        QVERIFY(report[u"passed"_s].toBool());
        // Paper geometry is deliberately independent of the three source shells.
        const auto layout =
                QJsonDocument::fromJson(readFile(u":/assets/models/music-case-v040.json"_s))
                        .object();
        const auto layers = layout[u"layers"_s].toObject();
        const double paper = layout[u"cover"_s].toObject()[u"z"_s].toDouble();
        QVERIFY(paper > layers[u"rearFront"_s].toDouble());
        QVERIFY(paper < layers[u"lidBack"_s].toDouble());
        QCOMPARE(layout[u"cover"_s].toObject()[u"width"_s].toDouble(), 3.06);
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl(u"qrc:/assets/models/Music_case_v040.qml"_s));
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        for (const auto &name : { "Frosted_Polymer", "Ivory_Edges", "Optical_Diffuser" })
            QVERIFY(object->findChild<QObject *>(u"Music_Case_V040__"_s + QString::fromUtf8(name)));
    }
    void malformedGeometryRejected()
    {
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, readGlb("not a model"_ba));
        auto mesh = readFile(u":/assets/models/meshes/meshes_0__mesh.mesh"_s);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, readQtMesh(mesh.left(130)));
        mesh[4] = 99;
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, readQtMesh(mesh));
        auto source = readGlb(readFile(u":/assets/models/music-case-v040.glb"_s));
        QVector<Batch> corrupt;
        for (int i = 0; i < 3; ++i)
            corrupt.append(
                    readQtMesh(readFile(u":/assets/models/meshes/meshes_%1__mesh.mesh"_s.arg(i))));
        corrupt[0].vertices[0][0] += 0.01f;
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, compareGeometry(source, corrupt));
    }
    void fontFacesAndShaping()
    {
        const auto manifest =
                QJsonDocument::fromJson(readFile(u":/assets/manifests/fonts.json"_s)).object();
        const QString chinese =
                u"\u4e00\u5f20\u4e00\u5f20\uff0c\u6162\u6162\u542c\u97f3\u4e50\u6536\u85cf\uff08\u4e13\u8f91\uff09"_s;
        const QString latin = u"AV To gyjp (Album) 0123456789"_s;
        const std::array<int, 4> weights{ 250, 330, 450, 630 };
        int index = 0;
        QSet<QByteArray> outlines;
        for (const auto &value : manifest[u"fonts"_s].toArray()) {
            const auto face = value.toObject();
            const auto bytes = readFile(":/assets/"_L1 + face[u"path"_s].toString());
            QRawFont raw(bytes, 24, QFont::PreferNoHinting);
            QVERIFY(raw.isValid());
            QVERIFY(raw.familyName().startsWith(u"MiSans"_s));
            const auto os2 = raw.fontTable("OS/2");
            QVERIFY(os2.size() >= 6);
            QCOMPARE(qFromBigEndian<quint16>(os2.constData() + 4), weights[index]);
            for (const auto ch : chinese + latin)
                QVERIFY2(raw.supportsCharacter(ch),
                         qPrintable(u"Missing MiSans glyph %1"_s.arg(ch)));
            QFont font = yalahoo::assetFont(face[u"cssWeight"_s].toInt());
            font.setPixelSize(24);
            font.setHintingPreference(QFont::PreferNoHinting);
            const auto selected = QRawFont::fromFont(font);
            // Native font engines may map SFNT weights to Qt's named weight classes.
            // Verify the selected physical face and its original SFNT weight instead.
            QCOMPARE(selected.weight(), raw.weight());
            QCOMPARE(selected.fontTable("OS/2"), os2);
            QVERIFY(selected.familyName().startsWith(u"MiSans"_s));
            const char *outlineTable = raw.fontTable("CFF ").isEmpty() ? "glyf" : "CFF ";
            QCOMPARE(selected.fontTable(outlineTable), raw.fontTable(outlineTable));
            auto measure = [&](const QString &text) {
                QTextLayout layout(text, font);
                layout.beginLayout();
                auto line = layout.createLine();
                line.setLineWidth(10000);
                layout.endLayout();
                QVERIFY(line.isValid());
                QVERIFY(line.naturalTextWidth() > 0);
                QVERIFY(line.naturalTextWidth() < 2000);
                for (const auto &run : layout.glyphRuns())
                    QCOMPARE(run.rawFont().fontTable(outlineTable), raw.fontTable(outlineTable));
            };
            measure(chinese);
            measure(latin);
            const auto outlinesData = raw.fontTable(outlineTable);
            QVERIFY(!outlinesData.isEmpty());
            outlines.insert(QCryptographicHash::hash(outlinesData, QCryptographicHash::Sha256));
            ++index;
        }
        QCOMPARE(outlines.size(), 4);
    }
    void icons()
    {
        for (const auto &theme : { u"day"_s, u"night"_s }) {
            for (const auto &name : { "play", "pause", "stop", "search", "settings", "folder",
                                      "overview", "overview-return" }) {
                QSvgRenderer renderer(u":/assets/icons/%1-%2.svg"_s.arg(name, theme));
                QVERIFY(renderer.isValid());
                QCOMPARE(renderer.viewBoxF(), QRectF(0, 0, 24, 24));
                QImage tiny(24, 24, QImage::Format_ARGB32_Premultiplied);
                tiny.fill(Qt::transparent);
                QPainter small(&tiny);
                renderer.render(&small);
                small.end();
                int painted = 0;
                for (int y = 0; y < 24; ++y)
                    for (int x = 0; x < 24; ++x)
                        if (qAlpha(tiny.pixel(x, y)))
                            ++painted;
                QVERIFY(painted > 15);
            }
        }
    }
    void silentInterfaceEvents()
    {
        const auto map =
                QJsonDocument::fromJson(readFile(u":/assets/audio/interface-events.json"_s))
                        .object();
        QCOMPARE(map[u"events"_s].toArray().size(), 17);
        QVERIFY(!map[u"typingPreviewUsed"_s].toBool());
        for (const auto &event : map[u"events"_s].toArray()) {
            QVERIFY(event.toObject()[u"source"_s].isNull());
            QVERIFY(!event.toObject()[u"enabled"_s].toBool());
        }
    }
    void audioLoadsWithoutPlayback_data()
    {
        QTest::addColumn<QString>("file");
        for (const auto &name :
             { "atmosphere.ogg", "motif.ogg", "pulse.ogg", "observatory-preview.mp3" })
            QTest::newRow(name) << QString::fromUtf8(name);
    }
    void audioLoadsWithoutPlayback()
    {
        QFETCH(QString, file);
        QMediaPlayer player;
        int playing = 0;
        connect(&player, &QMediaPlayer::playbackStateChanged, &player, [&](auto state) {
            if (state == QMediaPlayer::PlayingState)
                ++playing;
        });
        QVERIFY(player.audioOutput() == nullptr);
        player.setSource(QUrl("qrc:/assets/audio/"_L1 + file));
        QTRY_VERIFY_WITH_TIMEOUT(player.mediaStatus() == QMediaPlayer::LoadedMedia
                                         || player.mediaStatus() == QMediaPlayer::InvalidMedia,
                                 AsyncDeadlineMilliseconds);
        QCOMPARE(player.error(), QMediaPlayer::NoError);
        QCOMPARE(player.mediaStatus(), QMediaPlayer::LoadedMedia);
        QVERIFY(player.duration() > 1000);
        QCOMPARE(player.playbackState(), QMediaPlayer::StoppedState);
        QCOMPARE(playing, 0);
    }
};
QTEST_MAIN(AssetTest)
#include "AssetTest.moc"
