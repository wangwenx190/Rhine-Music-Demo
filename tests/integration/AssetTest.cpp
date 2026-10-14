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

using namespace yalahoo::assets;
class AssetTest : public QObject
{
    Q_OBJECT
    QJsonArray evidence;
private slots:
    void initTestCase()
    {
        QVERIFY(yalahoo::initializeAssetFonts());
    }
    void embeddedHashes()
    {
        const auto manifest =
                QJsonDocument::fromJson(readFile(":/assets/manifests/assets.json")).object();
        QCOMPARE(manifest["baselineCommit"].toString(),
                 QString("57f4f845c20cce646a15dbf4008c2a162aaaf4e2"));
        const auto entries = manifest["entries"].toArray();
        QCOMPARE(entries.size(), 47);
        for (const auto &value : entries) {
            const auto entry = value.toObject();
            const auto bytes = readFile(entry["qrc"].toString());
            QCOMPARE(bytes.size(), entry["bytes"].toInteger());
            QCOMPARE(QString::fromLatin1(
                             QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()),
                     entry["sha256"].toString());
            QVERIFY(!entry["path"].toString().contains("typing-preview"));
            QVERIFY(!entry["path"].toString().contains("app-icon"));
        }
        evidence.append(QJsonObject{
                { "check", "qrc-hashes" }, { "entries", entries.size() }, { "passed", true } });
    }
    void geometry()
    {
        QVector<Batch> imported;
        for (int i = 0; i < 3; ++i)
            imported.append(readQtMesh(
                    readFile(QString(":/assets/models/meshes/meshes_%1__mesh.mesh").arg(i))));
        const auto report =
                compareGeometry(readGlb(readFile(":/assets/models/music-case-v040.glb")), imported);
        QVERIFY(report["passed"].toBool());
        // Paper geometry is deliberately independent of the three source shells.
        const auto layout =
                QJsonDocument::fromJson(readFile(":/assets/models/music-case-v040.json")).object();
        const auto layers = layout["layers"].toObject();
        const double paper = layout["cover"].toObject()["z"].toDouble();
        QVERIFY(paper > layers["rearFront"].toDouble());
        QVERIFY(paper < layers["lidBack"].toDouble());
        QCOMPARE(layout["cover"].toObject()["width"].toDouble(), 3.06);
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl("qrc:/assets/models/Music_case_v040.qml"));
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        for (const auto &name : { "Frosted_Polymer", "Ivory_Edges", "Optical_Diffuser" })
            QVERIFY(object->findChild<QObject *>(QString("Music_Case_V040__") + name));
        evidence.append(QJsonObject{ { "check", "geometry" },
                                     { "report", report },
                                     { "paperGap", true },
                                     { "qmlImport", true } });
    }
    void malformedGeometryRejected()
    {
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, readGlb(QByteArray("not a model")));
        auto mesh = readFile(":/assets/models/meshes/meshes_0__mesh.mesh");
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, readQtMesh(mesh.left(130)));
        mesh[4] = 99;
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, readQtMesh(mesh));
        auto source = readGlb(readFile(":/assets/models/music-case-v040.glb"));
        QVector<Batch> corrupt;
        for (int i = 0; i < 3; ++i)
            corrupt.append(readQtMesh(
                    readFile(QString(":/assets/models/meshes/meshes_%1__mesh.mesh").arg(i))));
        corrupt[0].vertices[0][0] += 0.01f;
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, compareGeometry(source, corrupt));
    }
    void fontFacesAndShaping()
    {
        const auto manifest =
                QJsonDocument::fromJson(readFile(":/assets/manifests/fonts.json")).object();
        const QString chinese = QStringLiteral("一张一张，慢慢听音乐收藏（专辑）");
        const QString latin = QStringLiteral("AV To gyjp (Album) 0123456789");
        const std::array<int, 4> weights{ 250, 330, 450, 630 };
        int index = 0;
        QJsonArray faces;
        QSet<QByteArray> outlines;
        QImage preview(1100, 280, QImage::Format_ARGB32_Premultiplied);
        preview.fill(QColor("#efede7"));
        QPainter painter(&preview);
        painter.setPen(QColor("#232722"));
        for (const auto &value : manifest["fonts"].toArray()) {
            const auto face = value.toObject();
            const auto bytes = readFile(":/assets/" + face["path"].toString());
            QRawFont raw(bytes, 24, QFont::PreferNoHinting);
            QVERIFY(raw.isValid());
            QVERIFY(raw.familyName().startsWith("MiSans"));
            const auto os2 = raw.fontTable("OS/2");
            QVERIFY(os2.size() >= 6);
            QCOMPARE(qFromBigEndian<quint16>(os2.constData() + 4), weights[index]);
            for (const auto ch : chinese + latin)
                QVERIFY2(raw.supportsCharacter(ch),
                         qPrintable(QString("Missing MiSans glyph %1").arg(ch)));
            QFont font = yalahoo::assetFont(face["cssWeight"].toInt());
            font.setPixelSize(24);
            font.setHintingPreference(QFont::PreferNoHinting);
            const auto selected = QRawFont::fromFont(font);
            // Native font engines may map SFNT weights to Qt's named weight classes.
            // Verify the selected physical face and its original SFNT weight instead.
            QCOMPARE(selected.weight(), raw.weight());
            QCOMPARE(selected.fontTable("OS/2"), os2);
            QVERIFY(selected.familyName().startsWith("MiSans"));
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
                faces.append(QJsonObject{ { "face", face["weight"] },
                                          { "weight", weights[index] },
                                          { "nativeWeight", int(raw.weight()) },
                                          { "text", text },
                                          { "widthAt24px", line.naturalTextWidth() } });
            };
            measure(chinese);
            measure(latin);
            painter.setFont(font);
            painter.drawText(20, index * 65 + 30,
                             face["weight"].toString() + ": " + chinese + " · " + latin);
            const auto outlinesData = raw.fontTable(outlineTable);
            QVERIFY(!outlinesData.isEmpty());
            outlines.insert(QCryptographicHash::hash(outlinesData, QCryptographicHash::Sha256));
            ++index;
        }
        painter.end();
        QVERIFY(preview.save("fonts.png"));
        QCOMPARE(outlines.size(), 4);
        evidence.append(QJsonObject{
                { "check", "fonts" }, { "faces", faces }, { "distinctOutlineCount", 4 } });
    }
    void icons()
    {
        QImage preview(8 * 96, 2 * 96, QImage::Format_ARGB32_Premultiplied);
        preview.fill(Qt::transparent);
        QPainter painter(&preview);
        int row = 0;
        for (const auto &theme : { QString("day"), QString("night") }) {
            painter.fillRect(0, row * 96, preview.width(), 96,
                             theme == "day" ? QColor("#efede7") : QColor("#162234"));
            int col = 0;
            for (const auto &name : { "play", "pause", "stop", "search", "settings", "folder",
                                      "overview", "overview-return" }) {
                QSvgRenderer renderer(QString(":/assets/icons/%1-%2.svg").arg(name, theme));
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
                renderer.render(&painter, QRectF(col++ * 96 + 16, row * 96 + 16, 64, 64));
            }
            ++row;
        }
        painter.end();
        QVERIFY(preview.save("icons.png"));
        evidence.append(QJsonObject{ { "check", "icons" },
                                     { "themes", 2 },
                                     { "functionalPaths", 8 },
                                     { "viewBox", "0 0 24 24" },
                                     { "passed", true } });
    }
    void silentInterfaceEvents()
    {
        const auto map =
                QJsonDocument::fromJson(readFile(":/assets/audio/interface-events.json")).object();
        QCOMPARE(map["events"].toArray().size(), 17);
        QVERIFY(!map["typingPreviewUsed"].toBool());
        for (const auto &event : map["events"].toArray()) {
            QVERIFY(event.toObject()["source"].isNull());
            QVERIFY(!event.toObject()["enabled"].toBool());
        }
        evidence.append(QJsonObject{ { "check", "interface-audio" },
                                     { "silentEvents", 17 },
                                     { "pcmOrPreviewPlayback", false } });
    }
    void audioLoadsWithoutPlayback_data()
    {
        QTest::addColumn<QString>("file");
        for (const auto &name :
             { "atmosphere.ogg", "motif.ogg", "pulse.ogg", "observatory-preview.mp3" })
            QTest::newRow(name) << QString(name);
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
        player.setSource(QUrl("qrc:/assets/audio/" + file));
        QTRY_VERIFY_WITH_TIMEOUT(player.mediaStatus() == QMediaPlayer::LoadedMedia
                                         || player.mediaStatus() == QMediaPlayer::InvalidMedia,
                                 10000);
        QCOMPARE(player.error(), QMediaPlayer::NoError);
        QCOMPARE(player.mediaStatus(), QMediaPlayer::LoadedMedia);
        QVERIFY(player.duration() > 1000);
        QCOMPARE(player.playbackState(), QMediaPlayer::StoppedState);
        QCOMPARE(playing, 0);
        evidence.append(QJsonObject{ { "check", "audio" },
                                     { "file", file },
                                     { "durationMs", player.duration() },
                                     { "loaded", true },
                                     { "playingEvents", playing },
                                     { "audioOutput", false } });
    }
    void cleanupTestCase()
    {
        saveFile("assets-report.json",
                 QJsonDocument(QJsonObject{ { "qtVersion", qVersion() }, { "checks", evidence } })
                         .toJson());
    }
};
QTEST_MAIN(AssetTest)
#include "AssetTest.moc"
