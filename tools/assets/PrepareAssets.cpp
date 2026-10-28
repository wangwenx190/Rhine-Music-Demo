#include "AssetFiles.h"
#include <QJsonArray>
#include "AssetTransaction.h"
#include "GeometryAudit.h"
#include "VerificationWindow.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QProcess>
#include <QRawFont>
#include <QSet>
#include <QTextStream>
#include <stdexcept>

using namespace Qt::StringLiterals;
using namespace yalahoo::assets;

namespace {
// Native resources are the complete input set. Reimport stages only derived model files.
constexpr auto ToolStartDeadlineMilliseconds = 5000;
constexpr auto ToolCompletionDeadlineMilliseconds = 60000;
constexpr auto PhysicalFontProbePixels = 24;
constexpr auto ModelBatchCount = 3;
constexpr auto MeshPathPrefixCharacters = 7;
constexpr auto MaximumManifestBytes = 1024 * 1024;

QString hash(const QByteArray &bytes)
{
    return QString::fromUtf8(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
QString resourcePath(const QDir &root, const QString &relative)
{
    if (relative.isEmpty() || QDir::isAbsolutePath(relative)
        || relative != QDir::cleanPath(relative) || relative == ".."_L1
        || relative.startsWith(u"../"_s) || relative.contains('\\'_L1))
        throw std::runtime_error("Invalid native asset path");
    const auto resources = root.filePath(u"resources"_s);
    QString current = resources;
    if (QFileInfo(current).isSymLink() || QFileInfo(current).isJunction())
        throw std::runtime_error("Native resource root must not be a link");
    for (const auto &part : relative.split('/'_L1)) {
        current = QDir(current).filePath(part);
        const QFileInfo info(current);
        if (info.isSymLink() || info.isJunction())
            throw std::runtime_error("Native asset path must not contain links");
    }
    return current;
}
QJsonObject inventory(const QDir &root)
{
    const auto path = resourcePath(root, u"manifests/assets.json"_s);
    if (QFileInfo(path).size() > MaximumManifestBytes)
        throw std::runtime_error("Native asset manifest exceeds its input budget");
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(readFile(path), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        throw std::runtime_error("Invalid native asset manifest JSON");
    const auto manifest = document.object();
    if (manifest[u"schemaVersion"_s] != 1 || !manifest[u"entries"_s].isArray()
        || manifest[u"entries"_s].toArray().isEmpty())
        throw std::runtime_error("Invalid native asset manifest schema");
    QSet<QString> paths;
    for (const auto &value : manifest[u"entries"_s].toArray()) {
        if (!value.isObject())
            throw std::runtime_error("Native asset entry must be an object");
        const auto entry = value.toObject();
        const auto relative = entry[u"path"_s].toString();
        const auto bytes = readFile(resourcePath(root, relative));
        if (paths.contains(relative) || entry[u"qrc"_s] != ":/assets/"_L1 + relative
            || !entry[u"bytes"_s].isDouble() || entry[u"bytes"_s].toDouble() != bytes.size()
            || entry[u"sha256"_s].toString() != hash(bytes))
            throw std::runtime_error("Native asset content differs from its manifest");
        paths.insert(relative);
    }
    for (const auto &name : { u"Light"_s, u"Regular"_s, u"Demibold"_s, u"Bold"_s }) {
        const QRawFont font(readFile(resourcePath(root, u"fonts/MiSans-%1.otf"_s.arg(name))),
                            PhysicalFontProbePixels, QFont::PreferNoHinting);
        if (!font.isValid())
            throw std::runtime_error("Native MiSans font could not be loaded");
    }
    return manifest;
}
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
}

int main(int argc, char **argv)
{
    yalahoo::verification::prepareApplication();
    QGuiApplication app(argc, argv);
    try {
        const auto args = app.arguments();
        const bool verifyOnly = args.size() == 4 && args[3] == "--verify-inputs"_L1;
        if (args.size() != 3 && !verifyOnly)
            throw std::runtime_error(
                    "Usage: prepare-assets <repository-root> <selected-Qt-Kit> [--verify-inputs]");
        const auto canonicalRoot = QFileInfo(args[1]).canonicalFilePath();
        if (canonicalRoot.isEmpty() || !QFileInfo(args[1]).isDir())
            throw std::runtime_error("Invalid native resource repository");
        const QDir root(canonicalRoot);
        const QDir kit(args[2]);
        const auto tool = [&](const QStringList &names) {
#ifdef Q_OS_WIN
            const QString suffix = u".exe"_s;
#else
            const QString suffix;
#endif
            for (const auto &directory : { u"bin/"_s, u"libexec/"_s }) {
                for (const auto &name : names) {
                    const auto path = kit.filePath(directory + name + suffix);
                    if (QFileInfo(path).isFile())
                        return path;
                }
            }
            throw std::runtime_error("Required preparation tool absent from selected Kit");
        };
        if (execute(tool({ u"qmake"_s, u"qmake6"_s }), { u"-query"_s, u"QT_VERSION"_s }).trimmed()
            != QByteArray(qVersion()))
            throw std::runtime_error("Preparation tool and selected Kit must use the same Qt");
        auto manifest = inventory(root);
        if (verifyOnly) {
            QTextStream(stdout) << "Verified native resource inventory and fonts\n";
            return 0;
        }
        const auto modelPath = u"models/music-case-v040.glb"_s;
        const auto model = readFile(resourcePath(root, modelPath));
        AssetTransaction transaction(root);
        const auto output = transaction.importDirectory();
        if (!QDir().mkpath(output))
            throw std::runtime_error("Cannot create model import staging directory");
        const auto log = execute(
                tool({ u"balsam"_s }),
                { resourcePath(root, modelPath), u"-o"_s, output,
                  u"--disable-generateMeshLevelsOfDetail"_s, u"--disable-globalScale"_s,
                  u"--disable-optimizeMeshes"_s, u"--disable-optimizeGraph"_s,
                  u"--disable-joinIdenticalVertices"_s, u"--disable-improveCacheLocality"_s });
        auto entries = manifest[u"entries"_s].toArray();
        const auto stage = [&](const QString &relative, const QByteArray &bytes) {
            bool found = false;
            for (qsizetype index = 0; index < entries.size(); ++index) {
                auto entry = entries[index].toObject();
                if (entry[u"path"_s] != relative)
                    continue;
                entry[u"source"_s] = "resources/"_L1 + modelPath;
                entry[u"inputSha256"_s] = hash(model);
                entry[u"sha256"_s] = hash(bytes);
                entry[u"bytes"_s] = bytes.size();
                entries[index] = entry;
                found = true;
                break;
            }
            if (!found)
                throw std::runtime_error("Derived model output absent from native inventory");
            transaction.stage("resources/"_L1 + relative, bytes);
        };
        QVector<Batch> imported;
        for (int index = 0; index < ModelBatchCount; ++index) {
            const auto relative = u"models/meshes/meshes_%1__mesh.mesh"_s.arg(index);
            const auto bytes = readFile(output + '/'_L1 + relative.mid(MeshPathPrefixCharacters));
            imported.append(readQtMesh(bytes));
            stage(relative, bytes);
        }
        const auto qml = readFile(output + "/Music_case_v040.qml"_L1);
        if (qml.contains("scale:"_ba) || qml.count("Model {"_ba) != ModelBatchCount)
            throw std::runtime_error("Unexpected imported model transforms or batches");
        stage(u"models/Music_case_v040.qml"_s, qml);
        auto geometry = compareGeometry(readGlb(model), imported);
        geometry[u"inputSha256"_s] = hash(model);
        geometry[u"qtVersion"_s] = QString::fromUtf8(qVersion());
        geometry[u"importLog"_s] = QString::fromUtf8(log)
                                           .replace(output, u"<ASSET-IMPORT>"_s)
                                           .replace(root.absolutePath(), u"<REPOSITORY>"_s);
        stage(u"manifests/geometry.json"_s, QJsonDocument(geometry).toJson());
        manifest[u"entries"_s] = entries;
        manifest[u"qtVersion"_s] = QString::fromUtf8(qVersion());
        transaction.stage(u"resources/manifests/assets.json"_s, QJsonDocument(manifest).toJson());
        transaction.publish();
        QTextStream(stdout) << "Prepared native model; 3 shells / 1000 triangles verified\n";
        return 0;
    } catch (const std::exception &error) {
        QTextStream(stderr) << error.what() << '\n';
        return 1;
    }
}
