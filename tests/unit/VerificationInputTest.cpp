// Exercises the actual migration validators and asset publication failure boundaries without
// network access, rendering, or touching repository assets.
#include "ProbeInput.h"
#include "BrandingManifest.h"
#include "AssetTransaction.h"
#include "ShellFixture.h"
#include "RequiredOutput.h"
#include <filesystem>
#include <QTest>
#include <QTemporaryDir>

using namespace Qt::StringLiterals;
using namespace yalahoo;
namespace {
QJsonObject rulerInput()
{
    QJsonArray slots;
    for (int i = 0; i < 16; ++i)
        slots.append(QJsonObject{ { u"x"_s, i },
                                  { u"ordinal"_s, i },
                                  { u"height"_s, 1 },
                                  { u"alpha"_s, 1 },
                                  { u"enabled"_s, true },
                                  { u"active"_s, false } });
    const QJsonObject state{ { u"width"_s, 100 },
                             { u"scroll"_s, 0 },
                             { u"scrollVelocity"_s, 0 },
                             { u"selected"_s, 0 },
                             { u"slots"_s, slots } };
    const QJsonObject operation{
        { u"count"_s, 1 }, { u"row"_s, 0 }, { u"physical"_s, 0 }, { u"loop"_s, false }
    };
    const QJsonObject sample{ { u"op"_s, operation },
                              { u"state"_s, state },
                              { u"capture"_s, false } };
    const QJsonObject capture{ { u"dpr"_s, 1 },
                               { u"fps"_s, 60 },
                               { u"narrow"_s, false },
                               { u"night"_s, false },
                               { u"states"_s, QJsonArray{ sample } } };
    return { { u"captures"_s, QJsonArray{ capture } },
             { u"ordinalSamples"_s,
               QJsonArray{ QJsonObject{ { u"op"_s, QJsonObject{ { u"target"_s, 1 } } },
                                        { u"value"_s, 0 },
                                        { u"velocity"_s, 0 } } } } };
}
QJsonObject titleInput()
{
    const QJsonObject capture{
        { u"width"_s, 300 },
        { u"budget"_s, 150 },
        { u"dpr"_s, 1 },
        { u"size"_s, 29.75 },
        { u"text"_s, u"Title"_s },
        { u"night"_s, false },
        { u"state"_s,
          QJsonObject{ { u"size"_s, 29.75 }, { u"height"_s, 30 }, { u"glyphs"_s, QJsonArray() } } }
    };
    const QJsonObject track{ { u"fps"_s, 60 },
                             { u"op"_s, QJsonObject{ { u"target"_s, 60 } } },
                             { u"sample"_s,
                               QJsonObject{ { u"value"_s, QJsonArray{ 0 } },
                                            { u"velocity"_s, QJsonArray{ 0 } } } } };
    const QJsonObject motion{ { u"position"_s, 0 }, { u"velocity"_s, 0 } };
    return { { u"protocol"_s, u"source-structure-font-metrics-motion-v1"_s },
             { u"captures"_s, QJsonArray{ capture } },
             { u"tracks"_s, QJsonArray{ track } },
             { u"rolling"_s,
               QJsonArray{ QJsonObject{ { u"dt"_s, 0 },
                                        { u"entry"_s, motion },
                                        { u"entryBlur"_s, motion },
                                        { u"roll"_s, motion } } } } };
}

}
class VerificationInputTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(VerificationInputTest)
public:
    VerificationInputTest() = default;
private Q_SLOTS:
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
    void titleRejectsUnsafeDimensionsBeforePaddingAndDprScaling()
    {
        const auto valid = titleInput();
        probe::validateTitle(QJsonDocument(valid));
        QCOMPARE(probe::titleCaptureSize(valid[u"captures"_s].toArray()[0].toObject(), 2, 2),
                 QSize(302, 152));
        auto scaled = valid;
        auto fractionalDpr = valid[u"captures"_s].toArray()[0].toObject();
        fractionalDpr[u"dpr"_s] = 1.5;
        scaled[u"captures"_s] = QJsonArray{ fractionalDpr };
        probe::validateTitle(QJsonDocument(scaled));
        for (const auto key : { "width"_L1, "budget"_L1, "dpr"_L1 }) {
            for (const auto &invalid :
                 { QJsonValue(2147483647), QJsonValue(1e100), QJsonValue(-1), QJsonValue(0),
                   QJsonValue(u"300"_s), QJsonValue(true), QJsonValue(QJsonValue::Null) }) {
                auto source = valid;
                auto capture = valid[u"captures"_s].toArray()[0].toObject();
                capture[key] = invalid;
                source[u"captures"_s] = QJsonArray{ capture };
                QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                         probe::validateTitle(QJsonDocument(source)));
            }
        }
        for (const auto key : { "width"_L1, "budget"_L1 }) {
            auto capture = valid[u"captures"_s].toArray()[0].toObject();
            capture[key] = 150.5;
            auto source = valid;
            source[u"captures"_s] = QJsonArray{ capture };
            QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                     probe::validateTitle(QJsonDocument(source)));
        }
        auto capture = valid[u"captures"_s].toArray()[0].toObject();
        capture[u"width"_s] = 4096;
        capture[u"budget"_s] = 4096;
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, probe::titleCaptureSize(capture, 1, 2));
        capture[u"budget"_s] = 10;
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, probe::titleCaptureSize(capture, 2, 2));
        QCOMPARE(probe::titleCaptureSize(capture, 1, 2), QSize(4098, 12));
        for (const auto key : { "captures"_L1, "tracks"_L1, "rolling"_L1 }) {
            auto source = valid;
            source[key] = QJsonArray{ QJsonObject() };
            QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                     probe::validateTitle(QJsonDocument(source)));
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
    void rulerRejectsEmptyAndMalformedSourceComparisons()
    {
        const auto valid = rulerInput();
        probe::validateRuler(QJsonDocument(valid));
        for (const auto key : { "captures"_L1, "ordinalSamples"_L1 }) {
            auto damaged = valid;
            damaged[key] = QJsonArray();
            QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                     probe::validateRuler(QJsonDocument(damaged)));
            damaged[key] = QJsonArray{ QJsonObject() };
            QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                     probe::validateRuler(QJsonDocument(damaged)));
        }
        auto capture = valid[u"captures"_s].toArray()[0].toObject();
        auto damaged = valid;
        capture[u"states"_s] = QJsonArray();
        damaged[u"captures"_s] = QJsonArray{ capture };
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, probe::validateRuler(QJsonDocument(damaged)));
        for (const auto key : { "dpr"_L1, "fps"_L1, "narrow"_L1 }) {
            capture = valid[u"captures"_s].toArray()[0].toObject();
            capture[key] = u"1"_s;
            damaged[u"captures"_s] = QJsonArray{ capture };
            QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                     probe::validateRuler(QJsonDocument(damaged)));
        }
        capture = valid[u"captures"_s].toArray()[0].toObject();
        auto sample = capture[u"states"_s].toArray()[0].toObject();
        auto state = sample[u"state"_s].toObject();
        state.remove(u"scroll"_s);
        sample[u"state"_s] = state;
        capture[u"states"_s] = QJsonArray{ sample };
        damaged[u"captures"_s] = QJsonArray{ capture };
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, probe::validateRuler(QJsonDocument(damaged)));
        auto ordinal = valid[u"ordinalSamples"_s].toArray()[0].toObject();
        ordinal.remove(u"velocity"_s);
        damaged = valid;
        damaged[u"ordinalSamples"_s] = QJsonArray{ ordinal };
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, probe::validateRuler(QJsonDocument(damaged)));
    }
    void rulerBoundsIndividualAndAggregateSynchronousWork()
    {
        const auto input = [](int frames, int operations, int captures) {
            auto source = rulerInput();
            auto capture = source[u"captures"_s].toArray()[0].toObject();
            auto states = capture[u"states"_s].toArray();
            auto step = states.first().toObject();
            step[u"op"_s] = QJsonObject{ { u"frames"_s, frames } };
            for (int i = 0; i < operations; ++i)
                states.append(step);
            capture[u"states"_s] = states;
            QJsonArray cases;
            for (int i = 0; i < captures; ++i)
                cases.append(capture);
            source[u"captures"_s] = cases;
            return QJsonDocument(source);
        };
        probe::validateRuler(input(10000, 5, 2));
        for (const auto frames : { 10001, 2147483647 })
            QVERIFY_THROWS_EXCEPTION(std::runtime_error, probe::validateRuler(input(frames, 1, 1)));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, probe::validateRuler(input(10000, 11, 1)));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, probe::validateRuler(input(10000, 6, 2)));
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
    void rulerTraceMustContainEveryDeclaredFrame()
    {
        auto source = rulerInput();
        auto capture = source[u"captures"_s].toArray()[0].toObject();
        auto states = capture[u"states"_s].toArray();
        auto frame = states[0].toObject();
        frame[u"op"_s] = QJsonObject{ { u"frames"_s, 2 } };
        auto expected = frame[u"state"_s].toObject();
        expected[u"trace"_s] = QJsonArray{ expected };
        frame[u"state"_s] = expected;
        states.append(frame);
        capture[u"states"_s] = states;
        source[u"captures"_s] = QJsonArray{ capture };
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, probe::validateRuler(QJsonDocument(source)));
        expected[u"trace"_s] =
                QJsonArray{ states[0].toObject()[u"state"_s], states[0].toObject()[u"state"_s] };
        frame[u"state"_s] = expected;
        states[1] = frame;
        capture[u"states"_s] = states;
        source[u"captures"_s] = QJsonArray{ capture };
        probe::validateRuler(QJsonDocument(source));
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
