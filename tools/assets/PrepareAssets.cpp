#include "GeometryAudit.h"
#include "AssetTransaction.h"
#include "VerificationWindow.h"
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

using namespace Qt::StringLiterals;

#ifdef Q_OS_WIN
#  include <dwrite_3.h>
#  include <wrl/client.h>
#endif

using namespace yalahoo::assets;
namespace {
// Preparation validates the manifest inventory and bounds child-tool execution before publishing outputs.
#ifdef Q_OS_WIN
constexpr auto BytesPerKiB = 1024;
#endif
constexpr auto ToolStartDeadlineMilliseconds = 5000;
constexpr auto ToolCompletionDeadlineMilliseconds = 60000;
constexpr auto ExpectedAssetInputs = 24;
constexpr auto PhysicalFontProbePixels = 24;
constexpr auto SourcePathPrefixCharacters = 11;
constexpr auto ResourcePathPrefixCharacters = 7;

QString hash(const QByteArray &bytes)
{
    return QString::fromUtf8(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
#ifdef Q_OS_WIN
QByteArray unpackFont(const QByteArray &packed)
{
    Microsoft::WRL::ComPtr<IDWriteFactory5> factory;
    HRESULT result = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory5),
                                         reinterpret_cast<IUnknown **>(factory.GetAddressOf()));
    if (FAILED(result))
        throw std::runtime_error("DirectWrite Factory5 unavailable");
    Microsoft::WRL::ComPtr<IDWriteFontFileStream> stream;
    result = factory->UnpackFontFile(DWRITE_CONTAINER_TYPE_WOFF2, packed.constData(),
                                     UINT32(packed.size()), &stream);
    if (FAILED(result))
        throw std::runtime_error("DirectWrite WOFF2 unpack failed");
    UINT64 size = 0;
    result = stream->GetFileSize(&size);
    if (FAILED(result) || size > 64 * BytesPerKiB * BytesPerKiB)
        throw std::runtime_error("Invalid font size");
    const void *fragment = nullptr;
    void *context = nullptr;
    result = stream->ReadFileFragment(&fragment, 0, size, &context);
    if (FAILED(result))
        throw std::runtime_error("DirectWrite font stream read failed");
    QByteArray bytes(static_cast<const char *>(fragment), qsizetype(size));
    stream->ReleaseFileFragment(context);
    return bytes;
}
#endif
// Wait for a bounded child process and require its actual exit status before accepting output.
// A manifest hash or pre-existing file cannot substitute for a successful preparation command.
QByteArray execute(const QString &program, const QStringList &arguments)
{
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForStarted(ToolStartDeadlineMilliseconds)
        || !process.waitForFinished(ToolCompletionDeadlineMilliseconds)
        || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        throw std::runtime_error((u"Asset tool failed: "_s + program + '\n'_L1
                                  + QString::fromUtf8(process.readAllStandardError()))
                                         .toStdString());
    return process.readAllStandardOutput() + process.readAllStandardError();
}
// Validate every frozen input before the first generated file is written. Direct
// invocations and platform wrappers share this authoritative provenance guard.
void verifyFrozenInputs(const QDir &root)
{
    const QString original = u"57f4f845c20cce646a15dbf4008c2a162aaaf4e2"_s;
    const auto mappings = QJsonDocument::fromJson(
                                  readFile(root.filePath(u"docs/contracts/history-rewrite.json"_s)))
                                  .object()[u"revisions"_s]
                                  .toObject();
    const QString revision = mappings[original].toString(original);
    if (!QRegularExpression(u"^[a-f0-9]{40}$"_s).match(revision).hasMatch())
        throw std::runtime_error("Invalid frozen source revision");
    const auto manifest =
            QJsonDocument::fromJson(readFile(root.filePath(u"resources/manifests/assets.json"_s)))
                    .object();
    if (manifest[u"baselineCommit"_s].toString() != original)
        throw std::runtime_error("Unexpected asset baseline");
    QMap<QString, QString> inputs;
    auto addInput = [&](const QString &source, const QString &sha256) {
        if (!source.startsWith(u"legacy-web/"_s) || source.contains(u".."_s)
            || !QRegularExpression(u"^[a-f0-9]{64}$"_s).match(sha256).hasMatch()
            || (inputs.contains(source) && inputs[source] != sha256))
            throw std::runtime_error("Invalid asset input provenance");
        inputs.insert(source, sha256);
    };
    for (const auto &value : manifest[u"entries"_s].toArray()) {
        const auto entry = value.toObject();
        addInput(entry[u"source"_s].toString(), entry[u"inputSha256"_s].toString());
    }
    for (const auto &value : manifest[u"additionalInputs"_s].toArray()) {
        const auto entry = value.toObject();
        addInput(entry[u"source"_s].toString(), entry[u"sha256"_s].toString());
    }
    if (inputs.size() != ExpectedAssetInputs)
        throw std::runtime_error("Incomplete frozen asset inputs");
    for (auto input = inputs.cbegin(); input != inputs.cend(); ++input) {
        const auto frozen =
                execute(u"git"_s,
                        { u"-C"_s, root.absolutePath(), u"show"_s,
                          revision + ':'_L1 + input.key().mid(SourcePathPrefixCharacters) });
        if (hash(frozen) != input.value()
            || hash(readFile(root.filePath(input.key()))) != input.value())
            throw std::runtime_error(
                    ("Frozen asset input changed: "_L1 + input.key()).toStdString());
    }
}
}
int main(int argc, char **argv)
{
    yalahoo::verification::prepareApplication();
    QGuiApplication app(argc, argv);
    try {
        const auto args = app.arguments();
        const bool verifyInputsOnly = args.size() == 4 && args[3] == "--verify-inputs"_L1;
        if (args.size() != 3 && !verifyInputsOnly)
            throw std::runtime_error(
                    "Usage: prepare-assets <repository-root> <selected-Qt-Kit> [--verify-inputs]");
        const QDir root(args[1]);
        const QDir kit(args[2]);
#ifdef Q_OS_WIN
        const QString suffix = u".exe"_s;
#else
        const QString suffix;
#endif
        auto kitTool = [&](const QStringList &names) {
            for (const auto &directory : { u"bin/"_s, u"libexec/"_s })
                for (const auto &name : names) {
                    const auto path = kit.filePath(directory + name + suffix);
                    if (QFileInfo::exists(path))
                        return path;
                }
            throw std::runtime_error("Required preparation tool absent from selected Kit");
        };
        if (execute(kitTool({ u"qmake"_s, u"qmake6"_s }), { u"-query"_s, u"QT_VERSION"_s })
                    .trimmed()
            != QByteArray(qVersion()))
            throw std::runtime_error("Preparation tool and selected Kit must use the same Qt");
        verifyFrozenInputs(root);
        if (verifyInputsOnly) {
            QTextStream(stdout) << "Verified 24 frozen asset inputs before generation\n";
            return 0;
        }
        AssetTransaction transaction(root);
        QJsonArray entries, fonts;
        auto add = [&](const QString &path, const QByteArray &bytes, const QString &source,
                       const QString &operation) {
            transaction.stage("resources/"_L1 + path, bytes);
            const auto input = readFile(root.filePath(source));
            entries.append(QJsonObject{ { u"path"_s, path },
                                        { u"qrc"_s, ":/assets/"_L1 + path },
                                        { u"source"_s, source },
                                        { u"inputSha256"_s, hash(input) },
                                        { u"sha256"_s, hash(bytes) },
                                        { u"bytes"_s, bytes.size() },
                                        { u"operation"_s, operation } });
        };
        auto copy = [&](const QString &source, const QString &path) {
            add(path, readFile(root.filePath(source)), source, u"unchanged copy"_s);
        };
        const QString model = u"legacy-web/public/assets/music-case-v040.glb"_s;
        const auto glb = readFile(root.filePath(model));
        copy(model, u"models/music-case-v040.glb"_s);
        copy(u"legacy-web/art/music-case-v040.json"_s, u"models/music-case-v040.json"_s);
        const QString output = transaction.importDirectory();
        QDir().mkpath(output);
        const auto importLog = execute(
                kitTool({ u"balsam"_s }),
                { root.filePath(model), u"-o"_s, output, u"--disable-generateMeshLevelsOfDetail"_s,
                  u"--disable-globalScale"_s, u"--disable-optimizeMeshes"_s,
                  u"--disable-optimizeGraph"_s, u"--disable-joinIdenticalVertices"_s,
                  u"--disable-improveCacheLocality"_s });
        QVector<Batch> imported;
        for (int i = 0; i < 3; ++i) {
            const QString path = u"models/meshes/meshes_%1__mesh.mesh"_s.arg(i);
            const auto data = readFile(output + '/'_L1 + path.mid(ResourcePathPrefixCharacters));
            imported.append(readQtMesh(data));
            add(path, data, model, u"selected Kit balsam; preserve mesh topology and attributes"_s);
        }
        const auto qml = readFile(output + "/Music_case_v040.qml"_L1);
        if (qml.contains("scale:"_ba) || qml.count("Model {"_ba) != 3)
            throw std::runtime_error("Unexpected imported transforms/batches");
        add(u"models/Music_case_v040.qml"_s, qml, model,
            u"selected Kit balsam; original materials are import reference, R07 supplies production "
            "shading"_s);
        auto geometry = compareGeometry(readGlb(glb), imported);
        geometry.insert(u"inputSha256"_s, hash(glb));
        geometry.insert(u"qtVersion"_s, QString::fromUtf8(qVersion()));
        geometry.insert(u"importLog"_s,
                        QString::fromUtf8(importLog)
                                .replace(output, u"<ASSET-IMPORT>"_s)
                                .replace(root.absolutePath(), u"<REPOSITORY>"_s));
        add(u"manifests/geometry.json"_s, QJsonDocument(geometry).toJson(), model,
            u"independent indexed-attribute comparison"_s);
        const QString fontCssSource = u"legacy-web/src/style.css"_s;
        const auto fontCss = QString::fromUtf8(readFile(root.filePath(fontCssSource)));
        for (const auto &weight : { u"Light"_s, u"Regular"_s, u"Demibold"_s, u"Bold"_s }) {
            const QString source = "legacy-web/public/fonts/MiSans-"_L1 + weight + ".woff2"_L1;
            const auto packed = readFile(root.filePath(source));
            const int originalId = QFontDatabase::addApplicationFontFromData(packed);
            const bool originalLoaded = originalId >= 0;
            if (originalLoaded)
                QFontDatabase::removeApplicationFont(originalId);
            QByteArray bytes;
            if (originalLoaded)
                bytes = packed;
            else {
#ifdef Q_OS_WIN
                bytes = unpackFont(packed);
#else
                // The Windows preparation artifact is portable SFNT. Reuse it
                // only when both frozen WOFF2 and output match the manifest.
                const auto prior =
                        QJsonDocument::fromJson(
                                readFile(root.filePath(u"resources/manifests/assets.json"_s)))
                                .object()[u"entries"_s]
                                .toArray();
                for (const auto &entryValue : prior) {
                    const auto entry = entryValue.toObject();
                    if (entry[u"source"_s].toString() != source
                        || entry[u"operation"_s].toString().startsWith(u"unchanged"_s))
                        continue;
                    if (entry[u"inputSha256"_s].toString() != hash(packed))
                        throw std::runtime_error(
                                "Portable font input changed; regenerate with Windows DirectWrite");
                    const auto candidate =
                            readFile(root.filePath("resources/"_L1 + entry[u"path"_s].toString()));
                    if (entry[u"sha256"_s].toString() != hash(candidate))
                        throw std::runtime_error("Portable font output hash mismatch");
                    bytes = candidate;
                    break;
                }
                if (bytes.isEmpty())
                    throw std::runtime_error(
                            "Portable SFNT absent; prepare fonts once with Windows DirectWrite");
#endif
            }
            const QString extension = originalLoaded                       ? u".woff2"_s
                    : bytes.startsWith("OTTO"_ba)                          ? u".otf"_s
                    : bytes.startsWith(QByteArray::fromHex("00010000"_ba)) ? u".ttf"_s
                                                                           : QString();
            if (extension.isEmpty())
                throw std::runtime_error("Unexpected SFNT signature");
            const int id = QFontDatabase::addApplicationFontFromData(bytes);
            if (id < 0 || !QFontDatabase::applicationFontFamilies(id).contains(u"MiSans"_s))
                throw std::runtime_error("Prepared MiSans cannot load");
            QRawFont raw(bytes, PhysicalFontProbePixels, QFont::PreferNoHinting);
            const auto os2 = raw.fontTable("OS/2");
            if (!raw.isValid() || os2.size() < 6)
                throw std::runtime_error("Prepared font lacks OS/2 metadata");
            const auto cssMatch = QRegularExpression("MiSans-"_L1 + weight
                                                     + uR"(\.woff2[^}]+font-weight:\s*(\d+);)"_s)
                                          .match(fontCss);
            if (!cssMatch.hasMatch())
                throw std::runtime_error("Frozen CSS font mapping changed");
            QString styleName;
            const char *outlineTable = raw.fontTable("CFF ").isEmpty() ? "glyf" : "CFF ";
            for (const auto &style : QFontDatabase::styles(raw.familyName())) {
                const auto selected = QRawFont::fromFont(
                        QFontDatabase::font(raw.familyName(), style, PhysicalFontProbePixels));
                if (selected.fontTable(outlineTable) == raw.fontTable(outlineTable)) {
                    styleName = style;
                    break;
                }
            }
            if (styleName.isEmpty())
                throw std::runtime_error("Cannot explicitly select prepared font style");
            copy(source, "fonts/source/MiSans-"_L1 + weight + ".woff2"_L1);
            add("fonts/MiSans-"_L1 + weight + extension, bytes, source,
                originalLoaded ? u"Qt accepted original WOFF2"_s
                               : u"DirectWrite IDWriteFactory5::UnpackFontFile; lossless SFNT "
                                 "container unpack"_s);
            fonts.append(
                    QJsonObject{ { u"weight"_s, weight },
                                 { u"originalWoff2Loaded"_s, originalLoaded },
                                 { u"preparedLoaded"_s, true },
                                 { u"family"_s, raw.familyName() },
                                 { u"styleName"_s, styleName },
                                 { u"sfntWeight"_s, qFromBigEndian<quint16>(os2.constData() + 4) },
                                 { u"cssWeight"_s, cssMatch.captured(1).toInt() },
                                 { u"path"_s, "fonts/MiSans-"_L1 + weight + extension } });
        }
        copy(u"legacy-web/public/fonts/NOTICE.txt"_s, u"fonts/NOTICE.txt"_s);
        copy(u"legacy-web/public/fonts/MiSans-license.pdf"_s, u"fonts/MiSans-license.pdf"_s);
        add(u"manifests/fonts.json"_s,
            QJsonDocument(QJsonObject{ { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
                                       { u"fonts"_s, fonts } })
                    .toJson(),
            u"legacy-web/public/fonts/NOTICE.txt"_s,
            u"Qt load probe and portable font decisions"_s);
        const QString appSource = u"legacy-web/src/music-app.ts"_s;
        const auto script = QString::fromUtf8(readFile(root.filePath(appSource)));
        QMap<QString, QString> icons;
        const QString cssSource = u"legacy-web/src/music.css"_s;
        const auto css = QString::fromUtf8(readFile(root.filePath(cssSource)));
        const auto dayCss =
                QRegularExpression(uR"(\.music-app\s*\{([^}]+)\})"_s).match(css).captured(1);
        const auto nightCss =
                QRegularExpression(uR"(\.music-app\[data-theme="night"\]\s*\{([^}]+)\})"_s)
                        .match(css)
                        .captured(1);
        auto cssColor = [](const QString &block, const QString &variable) {
            const auto match = QRegularExpression("--"_L1 + variable + uR"(:\s*(#[a-fA-F0-9]+);)"_s)
                                       .match(block);
            if (!match.hasMatch())
                throw std::runtime_error("Theme icon color contract changed");
            return match.captured(1);
        };
        auto matches =
                QRegularExpression(uR"((\w+): svg\(\s*'([^']+)'\s*,?\s*\))"_s).globalMatch(script);
        while (matches.hasNext()) {
            const auto match = matches.next();
            icons.insert(match.captured(1), match.captured(2));
        }
        const auto inlineIcons =
                QRegularExpression(uR"(\$\{svg\('([^']+)'\)\})"_s).globalMatch(script);
        auto inlineMatches = inlineIcons;
        int inlineIndex = 0;
        while (inlineMatches.hasNext()) {
            icons.insert(inlineIndex++ == 0 ? u"overview"_s : u"overview-return"_s,
                         inlineMatches.next().captured(1));
        }
        if (icons.size() != 8 || inlineIndex != 2)
            throw std::runtime_error("Functional icon production call path changed");
        for (auto icon = icons.cbegin(); icon != icons.cend(); ++icon) {
            for (const auto &theme : { u"day"_s, u"night"_s }) {
                const QString ink = cssColor(theme == "day"_L1 ? dayCss : nightCss, u"ink"_s);
                const QString surface =
                        cssColor(theme == "day"_L1 ? dayCss : nightCss, u"surface"_s);
                const auto body = QString(icon.value())
                                          .replace(u"var(--surface)"_s, surface)
                                          .replace(u"currentColor"_s, ink);
                const auto svg =
                        u"<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" "
                        "fill=\"none\" stroke=\"%1\" stroke-width=\"1.5\" "
                        "stroke-linecap=\"round\" stroke-linejoin=\"round\">%2</svg>\n"_s.arg(ink,
                                                                                              body);
                add("icons/"_L1 + icon.key() + '-'_L1 + theme + ".svg"_L1, svg.toUtf8(), appSource,
                    u"extract actual svg() path; resolve currentColor and --surface from music.css"_s);
            }
        }
        for (const auto &stem : { u"atmosphere.ogg"_s, u"motif.ogg"_s, u"pulse.ogg"_s,
                                  u"observatory-preview.mp3"_s, u"score.json"_s })
            copy("legacy-web/public/audio/"_L1 + stem, "audio/"_L1 + stem);
        const QString audioSource = u"legacy-web/src/audio.ts"_s;
        const auto audio = QString::fromUtf8(readFile(root.filePath(audioSource)));
        const auto soundType = audio.mid(audio.indexOf(u"export type Sound ="_s),
                                         audio.indexOf(u"export type SoundScene"_s)
                                                 - audio.indexOf(u"export type Sound ="_s));
        QJsonArray events;
        auto sounds = QRegularExpression(u"\"([a-z-]+)\""_s).globalMatch(soundType);
        while (sounds.hasNext())
            events.append(QJsonObject{
                    { u"event"_s, sounds.next().captured(1) },
                    { u"source"_s, QJsonValue::Null },
                    { u"enabled"_s, false },
                    { u"reason"_s,
                      u"No standalone playable short asset; original uses synthesis or "
                      "embedded typing PCM. Preserve event and remain silent."_s } });
        if (events.size() != 17)
            throw std::runtime_error("Audio event production contract changed");
        add(u"audio/interface-events.json"_s,
            QJsonDocument(QJsonObject{ { u"events"_s, events }, { u"typingPreviewUsed"_s, false } })
                    .toJson(),
            audioSource,
            u"retain event names; no synthesis, PCM extraction or preview-per-key playback"_s);
        for (const auto &cover : { u"square.png"_s, u"portrait.png"_s, u"landscape.png"_s })
            copy("legacy-web/public/demo-covers/"_L1 + cover, "demo-covers/"_L1 + cover);
        copy(u"legacy-web/LICENSE"_s, u"licenses/project-LICENSE"_s);
        // Native attribution documents are maintained in place; frozen notices only establish
        // their provenance. Reimporting assets must not restore obsolete Web runtime claims.
        for (const auto &document :
             { u"audio/README.md"_s, u"demo-covers/README.md"_s, u"licenses/NOTICE.md"_s }) {
            const auto source = document.startsWith(u"licenses/"_s)
                    ? u"legacy-web/NOTICE.md"_s
                    : "legacy-web/public/"_L1 + document;
            add(document, readFile(root.filePath("resources/"_L1 + document)), source,
                u"Native attribution maintained in place; frozen source provenance retained"_s);
        }
        const QJsonObject manifest{
            { u"schemaVersion"_s, 1 },
            { u"baselineCommit"_s, u"57f4f845c20cce646a15dbf4008c2a162aaaf4e2"_s },
            { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
            { u"entries"_s, entries },
            { u"brandAssets"_s,
              u"resources/manifests/branding.json; original vectors; UI integration R14/R20"_s },
            { u"additionalInputs"_s,
              QJsonArray{ QJsonObject{ { u"source"_s, cssSource },
                                       { u"sha256"_s, hash(readFile(root.filePath(cssSource))) } },
                          QJsonObject{ { u"source"_s, fontCssSource },
                                       { u"sha256"_s,
                                         hash(readFile(root.filePath(fontCssSource))) } } } }
        };
        transaction.stage(u"resources/manifests/assets.json"_s, QJsonDocument(manifest).toJson());
        QByteArray cmake("# Generated by tools/assets/prepare-assets; resource aliases are "
                         "relative to resources/.\nset(YALAHOO_ASSET_FILES\n"_ba);
        for (const auto &entry : entries)
            cmake += "    \"${PROJECT_SOURCE_DIR}/resources/"_ba
                    + entry.toObject()[u"path"_s].toString().toUtf8() + "\"\n"_ba;
        cmake += "    \"${PROJECT_SOURCE_DIR}/resources/manifests/assets.json\"\n)\n"_ba;
        transaction.stage(u"resources/manifests/assets.cmake"_s, cmake);
        transaction.publish();
        QTextStream(stdout) << "Prepared " << entries.size() << " assets; Qt "
                            << QString::fromUtf8(qVersion())
                            << "; 3 shells / 1000 triangles verified\n";
        return 0;
    } catch (const std::exception &error) {
        QTextStream(stderr) << error.what() << '\n';
        return 1;
    }
}
