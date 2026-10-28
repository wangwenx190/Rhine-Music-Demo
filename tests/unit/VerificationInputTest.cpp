#include "AssetFiles.h"
#include "GeometryAudit.h"
#include "MaterialReviewInput.h"
#include "BoundedFile.h"
#include "NumericChecks.h"
#include "DeploymentImagePolicy.h"
#include "TitleReel.h"
#include <limits>
// Exercises native diagnostic validators and asset publication failure boundaries without
// network access, rendering, or touching repository assets.
#include "MetadataProbeInput.h"
#include "BrandingManifest.h"
#include "BrandingReportPath.h"
#include "FileFixture.h"
#include "AssetTransaction.h"
#include "ReviewOutput.h"
#include <QtEndian>
#include "ShellFixture.h"
#include "RequiredOutput.h"
#include <filesystem>
#include <QTest>
#include <QTemporaryDir>

using namespace Qt::StringLiterals;
using namespace yalahoo;
class VerificationInputTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(VerificationInputTest)
public:
    VerificationInputTest() = default;
private Q_SLOTS:
    void nativeReviewOutputPreservesExistingEvidence()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto output = directory.filePath(u"nested/output"_s);
        QVERIFY(review::prepareFreshOutput(output));
        assets::saveFile(output + u"/report.json"_s, "failed evidence"_ba);
        assets::saveFile(output + u"/frame.png"_s, "original pixels"_ba);
        QVERIFY(!review::prepareFreshOutput(output));
        QCOMPARE(assets::readFile(output + u"/report.json"_s), "failed evidence"_ba);
        QCOMPARE(assets::readFile(output + u"/frame.png"_s), "original pixels"_ba);
        QVERIFY(!review::prepareFreshOutput(output + u"/report.json/child"_s));
        QVERIFY(!review::prepareFreshOutput(QString()));
    }
    void meshDeclaredPayloadBoundsEveryRead()
    {
        const auto original = assets::readFile(QString::fromUtf8(FIXTURE_ROOT)
                                               + u"/resources/models/meshes/meshes_0__mesh.mesh"_s);
        QVERIFY(!assets::readQtMesh(original).vertices.isEmpty());
        const auto size = qFromLittleEndian<quint32>(original.constData() + 8);
        for (quint32 length : { 0u, 1u, size - 1, size + 1, size - 100 }) {
            auto malformed = original;
            qToLittleEndian(length, malformed.data() + 8);
            QVERIFY_THROWS_EXCEPTION(std::runtime_error, assets::readQtMesh(malformed));
        }
        auto malformed = original;
        malformed.chop(1);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, assets::readQtMesh(malformed));
    }
    void assetPublicationRollsBackWhenFinalReportFails()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QDir root(directory.path());
        assets::saveFile(root.filePath(u"resources/asset"_s), "original"_ba);
        assets::AssetTransaction batch(root);
        batch.stage(u"resources/asset"_s, "replacement"_ba);
        batch.stage(u"resources/new"_s, "new"_ba);
        QVERIFY_THROWS_EXCEPTION(
                std::runtime_error,
                batch.publish(
                        [](const QString &from, const QString &to) {
                            return QFile::rename(from, to);
                        },
                        [&] {
                            QCOMPARE(assets::readFile(root.filePath(u"resources/asset"_s)),
                                     "replacement"_ba);
                            // A real file write fails after publication, while backups must still exist.
                            assets::saveFile(root.path(), "report"_ba);
                        }));
        QCOMPARE(assets::readFile(root.filePath(u"resources/asset"_s)), "original"_ba);
        QVERIFY(!QFile::exists(root.filePath(u"resources/new"_s)));
        QVERIFY(!assets::brandingReportPathAllowed(root,
                                                   root.filePath(u"resources/asset/report"_s)));
    }
    void fixtureWritesPropagateOpenAndFlushFailures()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath(u"nested/input.bin"_s);
        QVERIFY(test::writeFile(path, "complete fixture"_ba));
        QCOMPARE(test::readFile(path), "complete fixture"_ba);
        QVERIFY(!test::writeFile(directory.path(), "cannot replace directory"_ba));
#ifdef Q_OS_LINUX
        if (QFileInfo::exists(u"/dev/full"_s))
            QVERIFY(!test::writeFile(u"/dev/full"_s, "buffered fixture"_ba));
#endif
    }
    void brandingReportsCannotAliasCurrentOrFutureProductOutputs()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QDir root(directory.path());
        for (const auto &path :
             { u"resources/branding/reveal-map.json"_s, u"RESOURCES/Branding/new.json"_s,
               u"resources/manifests/branding.json"_s, u"resources/icons/yalahoo/new.png"_s,
               u"docs/media/branding/new.png"_s, u".asset-transaction-123/import/new.png"_s })
            QVERIFY(!assets::brandingReportPathAllowed(root, root.filePath(path)));
        QVERIFY(assets::brandingReportPathAllowed(root,
                                                  root.filePath(u"verification/report.json"_s)));
        QVERIFY(QDir().mkpath(root.filePath(u"resources/branding"_s)));
#ifdef Q_OS_UNIX
        QVERIFY(QFile::link(root.filePath(u"resources/branding"_s), root.filePath(u"alias"_s)));
        QVERIFY(!assets::brandingReportPathAllowed(root, root.filePath(u"alias/new.json"_s)));
#endif
    }
    void windowsImageOriginsRequireRealDirectoryBoundaries()
    {
        QVERIFY(deployment::windowsImageUnder(u"C:/Package/Qt6Core.dll"_s, u"c:/package"_s));
        QVERIFY(!deployment::windowsImageUnder(u"C:/Package-copy/Qt6Core.dll"_s, u"C:/Package"_s));
        QVERIFY(!deployment::windowsImageUnder(u"C:/Package/../outside.dll"_s, u"C:/Package"_s));
        QVERIFY(!deployment::windowsImageUnder(QString(), QString()));
        QVERIFY(deployment::windowsSystemImage(u"C:/Windows/System32/kernel32.dll"_s,
                                               u"C:/Windows"_s));
        QVERIFY(deployment::windowsSystemImage(u"C:/Windows/WinSxS/runtime/vcruntime140.dll"_s,
                                               u"C:/Windows"_s));
        QVERIFY(!deployment::windowsSystemImage(u"C:/Windows/System32/Qt6Core.dll"_s,
                                                u"C:/Windows"_s));
        QVERIFY(!deployment::windowsSystemImage(u"C:/Windows/System32/avcodec-61.dll"_s,
                                                u"C:/Windows"_s));
        QVERIFY(!deployment::windowsSystemImage(u"C:/Windows/System32-extra/outside.dll"_s,
                                                u"C:/Windows"_s));
        QVERIFY(!deployment::windowsSystemImage(u"C:/Other/runtime.dll"_s, u"C:/Windows"_s));
    }
    void boundedInputsRejectExcessBytesBeforeParsing()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath(u"source.json"_s);
        assets::saveFile(path, "{}"_ba);
        QCOMPARE(readBoundedFile(path, 2).value(), "{}"_ba);
        QVERIFY(!readBoundedFile(path, 1));
        QVERIFY(!readBoundedFile(directory.filePath(u"missing"_s), 2));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.resize(review::MaximumMaterialInputBytes + 1));
        file.close();
        QVERIFY(!readBoundedFile(path, review::MaximumMaterialInputBytes));
    }
    void numericReductionsRejectInvalidMatrixAndOverflowedMotionSamples()
    {
        for (const double invalid :
             { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
               -std::numeric_limits<double>::infinity() }) {
            for (int component = 0; component < 16; ++component) {
                std::array<double, 16> actual{ };
                actual[component] = invalid;
                QVERIFY_THROWS_EXCEPTION(std::runtime_error, [&] {
                    double maximum = 0;
                    for (const auto value : actual)
                        maximum = std::max(maximum, test::errorMagnitude(value - 0));
                    return maximum;
                }());
            }
        }
        ReelTrack track;
        track.move(1e308);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, test::errorMagnitude(track.sample().value));
        QCOMPARE(test::errorMagnitude(-0.25), 0.25);
    }
    void materialFixturesRequireUniqueSurfacesAndFiniteProjectionInputs()
    {
        for (const auto &fixture : { u"materials/single-case.json"_s, u"lighting/visual.json"_s }) {
            const auto source =
                    QJsonDocument::fromJson(assets::readFile(QString::fromUtf8(FIXTURE_ROOT)
                                                             + u"/tests/fixtures/"_s + fixture))
                            .object();
            QVERIFY(review::validMaterialReview(source, 1));
            const auto original = source[u"cases"_s].toArray();
            for (int mutation = 0; mutation < 12; ++mutation) {
                auto damaged = source;
                auto cases = original;
                auto state = cases[0].toObject();
                auto parameters = state[u"parameters"_s].toArray();
                auto parameter = parameters[0].toObject();
                if (mutation == 0)
                    parameters[1] = parameters[0];
                else if (mutation == 1)
                    parameter[u"surface"_s] = u"Unknown"_s;
                else if (mutation == 2)
                    parameter.remove(u"surface"_s);
                else if (mutation == 3)
                    parameter[u"roughness"_s] = u"0.4"_s;
                else if (mutation == 4)
                    parameter.remove(u"transmission"_s);
                else if (mutation == 5)
                    parameter[u"thickness"_s] = -1;
                else if (mutation == 6)
                    state[u"position"_s] = QJsonArray{ 0, 1e300, 1 };
                else if (mutation == 7)
                    state[u"day"_s] = 1;
                else if (mutation == 8)
                    state[u"parameters"_s] = QJsonArray{ 0, 0, 0 };
                else {
                    auto projections = state[u"projections"_s].toArray();
                    auto point = projections[0].toObject();
                    if (mutation == 9)
                        point[u"world"_s] = QJsonArray{ 0, 1e300, 0 };
                    else if (mutation == 10)
                        point[u"pixel"_s] = QJsonArray{ 0 };
                    else
                        point[u"pixel"_s] = QJsonArray{ u"zero"_s, 0 };
                    projections[0] = point;
                    state[u"projections"_s] = projections;
                }
                parameters[0] = parameter;
                if (mutation != 8)
                    state[u"parameters"_s] = parameters;
                cases[0] = state;
                damaged[u"cases"_s] = cases;
                QVERIFY2(!review::validMaterialReview(damaged, 1),
                         qPrintable(QString::number(mutation)));
            }
            auto damaged = source;
            damaged[u"lighting"_s] = true;
            QJsonArray cases;
            auto baseline = original[0].toObject();
            baseline[u"mode"_s] = u"baseline"_s;
            for (int i = 0; i <= review::MaximumLightingCases; ++i)
                cases.append(baseline);
            damaged[u"cases"_s] = cases;
            QVERIFY(!review::validMaterialReview(damaged, 1));
            cases.removeLast();
            damaged[u"cases"_s] = cases;
            QVERIFY(review::validMaterialReview(damaged, 1));
            auto large = original[0].toObject();
            large[u"width"_s] = large[u"height"_s] = 4096;
            for (int i = 0; i < cases.size(); ++i)
                cases[i] = large;
            damaged[u"cases"_s] = cases;
            QVERIFY(!review::validMaterialReview(damaged, 1));
        }
    }
    void shellRequiresEverySourceIdentityAndProjectionSample()
    {
        const auto valid = QJsonDocument::fromJson(
                                   assets::readFile(QString::fromUtf8(FIXTURE_ROOT)
                                                    + u"/tests/fixtures/rhi-shell/visual.json"_s))
                                   .object();
        review::validateShellFixture(valid);
        const auto original = valid[u"cases"_s].toArray();
        for (int mutation = 0; mutation < 9; ++mutation) {
            auto source = valid;
            auto cases = original;
            auto state = cases[1].toObject();
            if (mutation == 0)
                state[u"sourceIndex"_s] = cases[2].toObject()[u"sourceIndex"_s];
            else if (mutation == 1)
                state[u"sourceIndex"_s] = 3;
            else if (mutation == 2)
                state[u"sourceIndex"_s] = 4.5;
            else if (mutation == 3)
                state.remove(u"projections"_s);
            else if (mutation == 4)
                state[u"projections"_s] = QJsonArray();
            else {
                auto projections = state[u"projections"_s].toArray();
                if (mutation == 5)
                    projections.removeLast();
                else {
                    auto point = projections[0].toObject();
                    point[mutation == 6 ? u"world"_s : u"pixel"_s] =
                            mutation == 8 ? QJsonArray{ u"invalid"_s, 0 } : QJsonArray{ 0 };
                    projections[0] = point;
                }
                state[u"projections"_s] = projections;
            }
            cases[1] = state;
            source[u"cases"_s] = cases;
            QVERIFY_THROWS_EXCEPTION(std::runtime_error, review::validateShellFixture(source));
        }
    }
    void outputDirectoryRejectsExistingEvidenceAndCreationFailure()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto output = directory.filePath(u"run"_s);
        QVERIFY(review::createFreshOutputDirectory(output));
        const auto report = QDir(output).filePath(u"report.json"_s);
        const auto image = QDir(output).filePath(u"product-restored.png"_s);
        assets::saveFile(report, "failed report"_ba);
        assets::saveFile(image, "original image"_ba);
        QVERIFY(!review::createFreshOutputDirectory(output));
        QCOMPARE(assets::readFile(report), "failed report"_ba);
        QCOMPARE(assets::readFile(image), "original image"_ba);
        QVERIFY(!review::createFreshOutputDirectory(report));
        QVERIFY(!review::createFreshOutputDirectory(report + u"/blocked"_s));
        QVERIFY(!review::createFreshOutputDirectory(QString()));
        const auto nested = directory.filePath(u"new-parent/run"_s);
        QVERIFY(review::createFreshOutputDirectory(nested));
    }
    void assetPublicationRejectsLinkedAncestorsWithoutTouchingExternalFiles()
    {
        QTemporaryDir directory, external;
        QVERIFY(directory.isValid());
        QVERIFY(external.isValid());
        const QDir root(directory.path());
        QVERIFY(root.mkpath(u"resources"_s));
        const auto link = root.filePath(u"resources/audio"_s);
        std::error_code error;
        std::filesystem::create_directory_symlink(
                std::filesystem::path(external.path().toStdU16String()),
                std::filesystem::path(link.toStdU16String()), error);
#if defined(Q_OS_WIN)
        if (error)
            QSKIP("Native symlink creation unavailable; target filesystem checks remain R36.");
#endif
        QVERIFY2(!error, error.message().c_str());
        const auto outside = external.filePath(u"atmosphere.ogg"_s);
        assets::saveFile(outside, "external original"_ba);
        const auto local = root.filePath(u"resources/local"_s);
        assets::saveFile(local, "local original"_ba);
        assets::AssetTransaction batch(root);
        batch.stage(u"resources/local"_s, "local replacement"_ba);
        batch.stage(u"resources/audio/atmosphere.ogg"_s, "external replacement"_ba);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, batch.publish());
        QCOMPARE(assets::readFile(outside), "external original"_ba);
        QCOMPARE(assets::readFile(local), "local original"_ba);
        assets::AssetTransaction missing(root);
        missing.stage(u"resources/audio/new/sub/file"_s, "new external file"_ba);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, missing.publish());
        QVERIFY(!QFileInfo::exists(external.filePath(u"new"_s)));
        for (const auto &relative : { QString(), u"."_s, u".."_s, u"../escape"_s })
            QVERIFY_THROWS_EXCEPTION(std::runtime_error, missing.stage(relative, "invalid"_ba));
    }
    void tracksKeepOptionalDefaultsAndRejectInvalidSuppliedFields()
    {
        const QJsonObject valid{ { u"title"_s, u"Public track"_s }, { u"number"_s, 1 } };
        probe::validateTracks(QJsonDocument(QJsonArray{ valid }));
        auto extended = valid;
        extended[u"duration"_s] = 200.5;
        extended[u"disc"_s] = 2;
        probe::validateTracks(QJsonDocument(QJsonArray{ extended }));
        for (const auto key : { "duration"_L1, "disc"_L1, "number"_L1 }) {
            for (const auto &invalid : { QJsonValue(u"200"_s), QJsonValue(QJsonValue::Null),
                                         QJsonValue(true), QJsonValue(-1), QJsonValue(1e100) }) {
                if (key == "duration"_L1 && invalid.isDouble() && invalid.toDouble() > 0)
                    continue; // Durations are finite doubles rather than integer indices.
                auto track = valid;
                track[key] = invalid;
                QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                         probe::validateTracks(QJsonDocument(QJsonArray{ track })));
            }
        }
        for (const auto key : { "disc"_L1, "number"_L1 }) {
            auto track = valid;
            track[key] = 1.5;
            QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                     probe::validateTracks(QJsonDocument(QJsonArray{ track })));
            track[key] = 0;
            QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                     probe::validateTracks(QJsonDocument(QJsonArray{ track })));
        }
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                 probe::validateTracks(QJsonDocument(QJsonObject())));
    }
    void brandingManifestMatchesCommittedAssetTrees()
    {
        const QDir root(QString::fromUtf8(FIXTURE_ROOT));
        assets::validateBrandingManifest(root,
                                         QJsonDocument::fromJson(assets::readFile(root.filePath(
                                                 u"resources/manifests/branding.json"_s))));
    }
    void brandingManifestRequiresEveryUniqueNormalizedPathAndMatchingContent()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QDir root(directory.path());
        QJsonArray entries;
        for (int i = 0; i < 36; ++i) {
            const auto path =
                    (i < 18 ? u"resources/branding/%1.svg"_s : u"resources/icons/yalahoo/%1.png"_s)
                            .arg(i);
            assets::saveFile(root.filePath(path), "anonymous asset"_ba);
            entries.append(QJsonObject{
                    { u"path"_s, path },
                    { u"bytes"_s, 15 },
                    { u"sha256"_s,
                      u"e6ad16c1a0ac24aecb40a193b2e744cff65921593cced68226d57ceaa14b5cea"_s } });
        }
        // Independent known SHA-256 of the fixture bytes, rather than the validator's result.
        const QJsonObject valid{ { u"schemaVersion"_s, 1 },
                                 { u"displayName"_s, u"yalahOO!!!"_s },
                                 { u"entries"_s, entries } };
        assets::validateBrandingManifest(root, QJsonDocument(valid));
        auto reversed = entries;
        for (int i = 0; i < entries.size(); ++i)
            reversed[i] = entries[entries.size() - 1 - i];
        auto manifest = valid;
        manifest[u"entries"_s] = reversed;
        assets::validateBrandingManifest(root, QJsonDocument(manifest));
        for (int mutation = 0; mutation < 9; ++mutation) {
            auto damaged = entries;
            auto entry = damaged[0].toObject();
            if (mutation == 0)
                damaged[1] = damaged[0];
            else if (mutation == 1)
                damaged.removeLast();
            else if (mutation == 2)
                entry[u"path"_s] = u"resources/branding/../branding/0.svg"_s;
            else if (mutation == 3)
                entry[u"path"_s] = u"resources/branding/missing.svg"_s;
            else if (mutation == 4)
                entry[u"bytes"_s] = u"15"_s;
            else if (mutation == 5)
                entry[u"bytes"_s] = 15.5;
            else if (mutation == 6)
                entry[u"sha256"_s] = QString();
            else if (mutation == 7)
                entry[u"path"_s] = root.filePath(u"resources/branding/0.svg"_s);
            else
                damaged[1] = QJsonValue();
            damaged[0] = entry;
            manifest[u"entries"_s] = damaged;
            QVERIFY_THROWS_EXCEPTION(
                    std::runtime_error,
                    assets::validateBrandingManifest(root, QJsonDocument(manifest)));
        }
        assets::saveFile(root.filePath(u"resources/branding/0.svg"_s), "changed content"_ba);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                 assets::validateBrandingManifest(root, QJsonDocument(valid)));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                 assets::validateBrandingManifest(root, QJsonDocument(entries)));
    }
    void assetRollbackFailureRetainsTheOnlyOriginalCopy()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QDir root(directory.path());
        const auto destination = root.filePath(u"resources/manifest"_s);
        assets::saveFile(destination, "original manifest"_ba);
        {
            assets::AssetTransaction batch(root);
            batch.stage(u"resources/manifest"_s, "candidate manifest"_ba);
            int renames = 0;
            try {
                batch.publish([&](const QString &from, const QString &to) {
                    if (++renames == 2) {
                        QDir().mkdir(to); // Block both publication and restoration.
                        return false;
                    }
                    return QFile::rename(from, to);
                });
                QFAIL("An incomplete rollback must report its recovery path");
            } catch (const std::runtime_error &error) {
                QVERIFY(QString::fromUtf8(error.what()).contains(u"retained recovery data"_s));
            }
        }
        const auto retained = root.entryList({ u".asset-transaction-*"_s },
                                             QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
        QCOMPARE(retained.size(), 1);
        QCOMPARE(assets::readFile(root.filePath(retained.first() + u"/old/resources/manifest"_s)),
                 "original manifest"_ba);
    }
    void assetBatchRetainsOriginalsBeforePublishAndRestoresMidPublishFailure()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QDir root(directory.path());
        assets::saveFile(root.filePath(u"resources/model"_s), "old model"_ba);
        assets::saveFile(root.filePath(u"resources/manifest"_s), "old manifest"_ba);
        {
            assets::AssetTransaction batch(root);
            batch.stage(u"resources/model"_s, "invalid candidate"_ba);
            // A later geometry/font validation failure simply destroys the candidate batch.
        }
        QCOMPARE(assets::readFile(root.filePath(u"resources/model"_s)), "old model"_ba);
        assets::AssetTransaction batch(root);
        batch.stage(u"resources/model"_s, "new model"_ba);
        batch.stage(u"resources/new-file"_s, "new asset"_ba);
        batch.stage(u"resources/manifest"_s, "new manifest"_ba);
        int renames = 0;
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                 batch.publish([&](const QString &from, const QString &to) {
                                     // Fail after two successful installs and after retaining the old manifest.
                                     return ++renames != 5 && QFile::rename(from, to);
                                 }));
        QCOMPARE(renames, 5);
        QCOMPARE(assets::readFile(root.filePath(u"resources/model"_s)), "old model"_ba);
        QCOMPARE(assets::readFile(root.filePath(u"resources/manifest"_s)), "old manifest"_ba);
        QVERIFY(!QFile::exists(root.filePath(u"resources/new-file"_s)));
        assets::AssetTransaction success(root);
        success.stage(u"resources/model"_s, "new model"_ba);
        success.stage(u"resources/manifest"_s, "new manifest"_ba);
        success.publish();
        QCOMPARE(assets::readFile(root.filePath(u"resources/model"_s)), "new model"_ba);
        QCOMPARE(assets::readFile(root.filePath(u"resources/manifest"_s)), "new manifest"_ba);
    }
};
QTEST_GUILESS_MAIN(VerificationInputTest)
#include "VerificationInputTest.moc"
