// Exercises the actual migration validators and asset publication failure boundaries without
// network access, rendering, or touching repository assets.
#include "ProbeInput.h"
#include "AssetTransaction.h"
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
}
class VerificationInputTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(VerificationInputTest)
public:
    VerificationInputTest() = default;
private Q_SLOTS:
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
