#include "GeometryAudit.h"
#include <QCryptographicHash>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QProcess>
#include <QRawFont>
#include <QRegularExpression>
#include <QTextStream>
#include <QtEndian>
#include <QDir>
#include <QFileInfo>
#include <stdexcept>
#ifdef Q_OS_WIN
#include <dwrite_3.h>
#include <wrl/client.h>
#endif

using namespace yalahoo::assets;
namespace {
QString hash(const QByteArray &bytes) { return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()); }
#ifdef Q_OS_WIN
QByteArray unpackFont(const QByteArray &packed) {
    Microsoft::WRL::ComPtr<IDWriteFactory5> factory;
    HRESULT result = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory5), reinterpret_cast<IUnknown **>(factory.GetAddressOf()));
    if (FAILED(result)) throw std::runtime_error("DirectWrite Factory5 unavailable");
    Microsoft::WRL::ComPtr<IDWriteFontFileStream> stream;
    result = factory->UnpackFontFile(DWRITE_CONTAINER_TYPE_WOFF2, packed.constData(), UINT32(packed.size()), &stream);
    if (FAILED(result)) throw std::runtime_error("DirectWrite WOFF2 unpack failed");
    UINT64 size = 0; result = stream->GetFileSize(&size);
    if (FAILED(result) || size > 64 * 1024 * 1024) throw std::runtime_error("Invalid font size");
    const void *fragment = nullptr; void *context = nullptr;
    result = stream->ReadFileFragment(&fragment, 0, size, &context);
    if (FAILED(result)) throw std::runtime_error("DirectWrite font stream read failed");
    QByteArray bytes(static_cast<const char *>(fragment), qsizetype(size));
    stream->ReleaseFileFragment(context); return bytes;
}
#endif
QByteArray execute(const QString &program, const QStringList &arguments) {
    QProcess process; process.start(program, arguments);
    if (!process.waitForStarted(5000) || !process.waitForFinished(60000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        throw std::runtime_error((QString("Asset tool failed: ") + program + '\n' + process.readAllStandardError()).toStdString());
    return process.readAllStandardOutput() + process.readAllStandardError();
}
}
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    try {
        const auto args = app.arguments();
        if (args.size() != 3) throw std::runtime_error("Usage: prepare-assets <repository-root> <selected-Qt-Kit>");
        const QDir root(args[1]); const QDir kit(args[2]);
#ifdef Q_OS_WIN
        const QString suffix = ".exe";
#else
        const QString suffix;
#endif
        auto kitTool = [&](const QStringList &names) {
            for (const auto &directory : {QString("bin/"), QString("libexec/")})
                for (const auto &name : names) {
                    const auto path = kit.filePath(directory + name + suffix);
                    if (QFileInfo::exists(path)) return path;
                }
            throw std::runtime_error("Required preparation tool absent from selected Kit");
        };
        if (execute(kitTool({"qmake", "qmake6"}), {"-query", "QT_VERSION"}).trimmed() != qVersion())
            throw std::runtime_error("Preparation tool and selected Kit must use the same Qt");
        QJsonArray entries, fonts;
        auto add = [&](const QString &path, const QByteArray &bytes, const QString &source, const QString &operation) {
            saveFile(root.filePath("resources/" + path), bytes);
            const auto input = readFile(root.filePath(source));
            entries.append(QJsonObject{{"path", path}, {"qrc", ":/assets/" + path}, {"source", source},
                {"inputSha256", hash(input)}, {"sha256", hash(bytes)}, {"bytes", bytes.size()}, {"operation", operation}});
        };
        auto copy = [&](const QString &source, const QString &path) { add(path, readFile(root.filePath(source)), source, "unchanged copy"); };
        const QString model = "legacy-web/public/assets/music-case-v040.glb";
        const auto glb = readFile(root.filePath(model));
        copy(model, "models/music-case-v040.glb");
        copy("legacy-web/art/music-case-v040.json", "models/music-case-v040.json");
        const QString output = root.filePath("build/assets-import"); QDir().mkpath(output);
        const auto importLog = execute(kitTool({"balsam"}), {root.filePath(model), "-o", output,
            "--disable-generateMeshLevelsOfDetail", "--disable-globalScale", "--disable-optimizeMeshes", "--disable-optimizeGraph",
            "--disable-joinIdenticalVertices", "--disable-improveCacheLocality"});
        QVector<Batch> imported;
        for (int i = 0; i < 3; ++i) {
            const QString path = QString("models/meshes/meshes_%1__mesh.mesh").arg(i);
            const auto data = readFile(output + '/' + path.mid(7));
            imported.append(readQtMesh(data)); add(path, data, model, "selected Kit balsam; preserve mesh topology and attributes");
        }
        const auto qml = readFile(output + "/Music_case_v040.qml");
        if (qml.contains("scale:") || qml.count("Model {") != 3) throw std::runtime_error("Unexpected imported transforms/batches");
        add("models/Music_case_v040.qml", qml, model, "selected Kit balsam; original materials are import reference, R07 supplies production shading");
        auto geometry = compareGeometry(readGlb(glb), imported);
        geometry.insert("inputSha256", hash(glb)); geometry.insert("qtVersion", qVersion());
        geometry.insert("importLog", QString::fromUtf8(importLog).replace(root.absolutePath(), "<REPOSITORY>"));
        add("manifests/geometry.json", QJsonDocument(geometry).toJson(), model, "independent indexed-attribute comparison");
        const QString fontCssSource = "legacy-web/src/style.css";
        const auto fontCss = QString::fromUtf8(readFile(root.filePath(fontCssSource)));
        for (const auto &weight : {QString("Light"), QString("Regular"), QString("Demibold"), QString("Bold")}) {
            const QString source = "legacy-web/public/fonts/MiSans-" + weight + ".woff2";
            const auto packed = readFile(root.filePath(source));
            const int originalId = QFontDatabase::addApplicationFontFromData(packed);
            const bool originalLoaded = originalId >= 0;
            if (originalLoaded) QFontDatabase::removeApplicationFont(originalId);
            QByteArray bytes;
            if (originalLoaded) bytes = packed;
            else {
#ifdef Q_OS_WIN
                bytes = unpackFont(packed);
#else
                // The Windows preparation artifact is portable SFNT. Reuse it
                // only when both frozen WOFF2 and output match the manifest.
                const auto prior = QJsonDocument::fromJson(readFile(root.filePath("resources/manifests/assets.json"))).object()["entries"].toArray();
                for (const auto &entryValue : prior) {
                    const auto entry = entryValue.toObject();
                    if (entry["source"].toString() != source || entry["operation"].toString().startsWith("unchanged")) continue;
                    if (entry["inputSha256"].toString() != hash(packed)) throw std::runtime_error("Portable font input changed; regenerate with Windows DirectWrite");
                    const auto candidate = readFile(root.filePath("resources/" + entry["path"].toString()));
                    if (entry["sha256"].toString() != hash(candidate)) throw std::runtime_error("Portable font output hash mismatch");
                    bytes = candidate; break;
                }
                if (bytes.isEmpty()) throw std::runtime_error("Portable SFNT absent; prepare fonts once with Windows DirectWrite");
#endif
            }
            const QString extension = originalLoaded ? ".woff2" : bytes.startsWith("OTTO") ? ".otf" : bytes.startsWith(QByteArray::fromHex("00010000")) ? ".ttf" : QString();
            if (extension.isEmpty()) throw std::runtime_error("Unexpected SFNT signature");
            const int id = QFontDatabase::addApplicationFontFromData(bytes);
            if (id < 0 || !QFontDatabase::applicationFontFamilies(id).contains("MiSans")) throw std::runtime_error("Prepared MiSans cannot load");
            QRawFont raw(bytes, 24, QFont::PreferNoHinting);
            const auto os2 = raw.fontTable("OS/2");
            if (!raw.isValid() || os2.size() < 6) throw std::runtime_error("Prepared font lacks OS/2 metadata");
            const auto cssMatch = QRegularExpression("MiSans-" + weight + R"(\.woff2[^}]+font-weight:\s*(\d+);)").match(fontCss);
            if (!cssMatch.hasMatch()) throw std::runtime_error("Frozen CSS font mapping changed");
            QString styleName;
            const char *outlineTable = raw.fontTable("CFF ").isEmpty() ? "glyf" : "CFF ";
            for (const auto &style : QFontDatabase::styles(raw.familyName())) {
                const auto selected = QRawFont::fromFont(QFontDatabase::font(raw.familyName(), style, 24));
                if (selected.fontTable(outlineTable) == raw.fontTable(outlineTable)) { styleName = style; break; }
            }
            if (styleName.isEmpty()) throw std::runtime_error("Cannot explicitly select prepared font style");
            copy(source, "fonts/source/MiSans-" + weight + ".woff2");
            add("fonts/MiSans-" + weight + extension, bytes, source, originalLoaded ? "Qt accepted original WOFF2" : "DirectWrite IDWriteFactory5::UnpackFontFile; lossless SFNT container unpack");
            fonts.append(QJsonObject{{"weight", weight}, {"originalWoff2Loaded", originalLoaded}, {"preparedLoaded", true},
                {"family", raw.familyName()}, {"styleName", styleName}, {"sfntWeight", qFromBigEndian<quint16>(os2.constData() + 4)},
                {"cssWeight", cssMatch.captured(1).toInt()}, {"path", "fonts/MiSans-" + weight + extension}});
        }
        copy("legacy-web/public/fonts/NOTICE.txt", "fonts/NOTICE.txt");
        copy("legacy-web/public/fonts/MiSans-license.pdf", "fonts/MiSans-license.pdf");
        add("manifests/fonts.json", QJsonDocument(QJsonObject{{"qtVersion", qVersion()}, {"fonts", fonts}}).toJson(),
            "legacy-web/public/fonts/NOTICE.txt", "Qt load probe and portable font decisions");
        const QString appSource = "legacy-web/src/music-app.ts";
        const auto script = QString::fromUtf8(readFile(root.filePath(appSource)));
        QMap<QString, QString> icons;
        const QString cssSource = "legacy-web/src/music.css";
        const auto css = QString::fromUtf8(readFile(root.filePath(cssSource)));
        const auto dayCss = QRegularExpression(R"(\.music-app\s*\{([^}]+)\})").match(css).captured(1);
        const auto nightCss = QRegularExpression(R"(\.music-app\[data-theme="night"\]\s*\{([^}]+)\})").match(css).captured(1);
        auto cssColor = [](const QString &block, const QString &variable) {
            const auto match = QRegularExpression("--" + variable + R"(:\s*(#[a-fA-F0-9]+);)").match(block);
            if (!match.hasMatch()) throw std::runtime_error("Theme icon color contract changed");
            return match.captured(1);
        };
        auto matches = QRegularExpression(R"((\w+): svg\(\s*'([^']+)'\s*,?\s*\))").globalMatch(script);
        while (matches.hasNext()) { const auto match = matches.next(); icons.insert(match.captured(1), match.captured(2)); }
        const auto inlineIcons = QRegularExpression(R"(\$\{svg\('([^']+)'\)\})").globalMatch(script);
        auto inlineMatches = inlineIcons; int inlineIndex = 0;
        while (inlineMatches.hasNext()) { icons.insert(inlineIndex++ == 0 ? "overview" : "overview-return", inlineMatches.next().captured(1)); }
        if (icons.size() != 8 || inlineIndex != 2) throw std::runtime_error("Functional icon production call path changed");
        for (auto icon = icons.cbegin(); icon != icons.cend(); ++icon) {
            for (const auto &theme : {QString("day"), QString("night")}) {
                const QString ink = cssColor(theme == "day" ? dayCss : nightCss, "ink");
                const QString surface = cssColor(theme == "day" ? dayCss : nightCss, "surface");
                const auto body = QString(icon.value()).replace("var(--surface)", surface).replace("currentColor", ink);
                const auto svg = QString("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"%1\" stroke-width=\"1.5\" stroke-linecap=\"round\" stroke-linejoin=\"round\">%2</svg>\n").arg(ink, body);
                add("icons/" + icon.key() + '-' + theme + ".svg", svg.toUtf8(), appSource, "extract actual svg() path; resolve currentColor and --surface from music.css");
            }
        }
        for (const auto &stem : {QString("atmosphere.ogg"), QString("motif.ogg"), QString("pulse.ogg"), QString("observatory-preview.mp3"), QString("score.json"), QString("README.md")})
            copy("legacy-web/public/audio/" + stem, "audio/" + stem);
        const QString audioSource = "legacy-web/src/audio.ts";
        const auto audio = QString::fromUtf8(readFile(root.filePath(audioSource)));
        const auto soundType = audio.mid(audio.indexOf("export type Sound ="), audio.indexOf("export type SoundScene") - audio.indexOf("export type Sound ="));
        QJsonArray events; auto sounds = QRegularExpression("\"([a-z-]+)\"").globalMatch(soundType);
        while (sounds.hasNext()) events.append(QJsonObject{{"event", sounds.next().captured(1)}, {"source", QJsonValue::Null},
            {"enabled", false}, {"reason", "No standalone playable short asset; original uses synthesis or embedded typing PCM. Preserve event and remain silent."}});
        if (events.size() != 17) throw std::runtime_error("Audio event production contract changed");
        add("audio/interface-events.json", QJsonDocument(QJsonObject{{"events", events}, {"typingPreviewUsed", false}}).toJson(), audioSource, "retain event names; no synthesis, PCM extraction or preview-per-key playback");
        for (const auto &cover : {QString("square.png"), QString("portrait.png"), QString("landscape.png"), QString("README.md")})
            copy("legacy-web/public/demo-covers/" + cover, "demo-covers/" + cover);
        copy("legacy-web/LICENSE", "licenses/project-LICENSE"); copy("legacy-web/NOTICE.md", "licenses/NOTICE.md");
        const QJsonObject manifest{{"schemaVersion", 1}, {"baselineCommit", "57f4f845c20cce646a15dbf4008c2a162aaaf4e2"},
            {"qtVersion", qVersion()}, {"entries", entries}, {"brandAssets", "resources/manifests/branding.json; original vectors; UI integration R14/R20"},
            {"additionalInputs", QJsonArray{QJsonObject{{"source", cssSource}, {"sha256", hash(readFile(root.filePath(cssSource)))}},
                QJsonObject{{"source", fontCssSource}, {"sha256", hash(readFile(root.filePath(fontCssSource)))}}}}};
        saveFile(root.filePath("resources/manifests/assets.json"), QJsonDocument(manifest).toJson());
        QByteArray cmake("# Generated by tools/assets/prepare-assets; resource aliases are relative to resources/.\nset(YALAHOO_ASSET_FILES\n");
        for (const auto &entry : entries) cmake += "    \"${PROJECT_SOURCE_DIR}/resources/" + entry.toObject()["path"].toString().toUtf8() + "\"\n";
        cmake += "    \"${PROJECT_SOURCE_DIR}/resources/manifests/assets.json\"\n)\n";
        saveFile(root.filePath("resources/manifests/assets.cmake"), cmake);
        QTextStream(stdout) << "Prepared " << entries.size() << " assets; Qt " << qVersion() << "; 3 shells / 1000 triangles verified\n";
        return 0;
    } catch (const std::exception &error) { QTextStream(stderr) << error.what() << '\n'; return 1; }
}
