// Verifies whole-run shaping, interrupted slot motion and visibility/cache lifecycle on
// anonymous text. Explicit fixture literals are independent inputs or expected results; timing
// bounds have named constants.

#include "FrameClock.h"
#include "TextLayoutAdapter.h"
#include "TitleReel.h"
#include "TransportMotion.h"
#include "AssetCatalog.h"
#include <QTest>
#include <QFontMetricsF>
#include <QAbstractItemModelTester>
#include <QTextLayout>
#include <QRawFont>
#include <QSignalSpy>
#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

class TitleTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TitleTest)
public:
    TitleTest() = default;

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(yalahoo::initializeAssetFonts());
    }
    void graphemesAndParts()
    {
        const QString text =
                u"A\U0001f468\u200d\U0001f469\u200d\U0001f467\u200d\U0001f466\U0001f44d\U0001f3fde\u0301\u97f3\u4e50\uff08gyp\uff09"_s;
        const auto boundaries = yalahoo::TextLayoutAdapter::boundaries(text);
        QCOMPARE(boundaries.size(), 12);
        QCOMPARE(text.mid(boundaries[1], boundaries[2] - boundaries[1]),
                 u"\U0001f468\u200d\U0001f469\u200d\U0001f467\u200d\U0001f466"_s);
        QCOMPARE(text.mid(boundaries[2], boundaries[3] - boundaries[2]), u"\U0001f44d\U0001f3fd"_s);
        QCOMPARE(text.mid(boundaries[3], boundaries[4] - boundaries[3]), u"e\u0301"_s);
        auto parts = yalahoo::TextLayoutAdapter::titleParts(
                u"\u97f3\u4e50\u6536\u85cf\uff08Archive gyp\uff09"_s);
        QCOMPARE(parts.main, u"\u97f3\u4e50\u6536\u85cf"_s);
        QCOMPARE(parts.translation, u"Archive gyp"_s);
        QCOMPARE(yalahoo::TextLayoutAdapter::titleParts(u"Music (Live)"_s).translation, QString());
        QCOMPARE(yalahoo::TextLayoutAdapter::titleParts(
                         u"\u97f3\u4e50\uff08\u73b0\u573a\u7248\uff09"_s)
                         .translation,
                 QString());
    }
    void layoutAndCache()
    {
        yalahoo::TextLayoutAdapter adapter;
        const auto bold = yalahoo::assetFont(700), subtitle = yalahoo::assetFont(600);
        auto a = adapter.measure(u"\u97f3\u4e50\u6536\u85cf\uff08Archive gyp\uff09"_s, bold,
                                 subtitle, 28, 270, 180, false, 1);
        QVERIFY(a.height > 28 * 1.4);
        QVERIFY(a.glyphs.last().translation);
        const auto misses = adapter.misses();
        auto b = adapter.measure(u"\u97f3\u4e50\u6536\u85cf\uff08Archive gyp\uff09"_s, bold,
                                 subtitle, 28, 270, 180, false, 1);
        QCOMPARE(a.glyphs.size(), b.glyphs.size());
        QCOMPARE(adapter.misses(), misses);
        for (const auto &glyph : a.glyphs) {
            QVERIFY(glyph.height >= glyph.size * 1.4);
            QVERIFY(glyph.width >= 0);
            QVERIFY(!glyph.renderRuns.isEmpty());
        }
        const auto longTitle =
                u"\u6536\u85cf\u7684\u97f3\u4e50\u58f0\u97f3\u548c\u5f80\u65e5\u7684\u6545\u4e8b\uff08The collected archive of music gyp\uff09"_s
                        .repeated(20);
        auto limited = adapter.measure(longTitle, bold, subtitle, 28, 150, 65, false, 2);
        QVERIFY(limited.overflow);
        QCOMPARE(limited.size, 20.0);
        QCOMPARE(limited.height, 65.0);
        bool ellipsis = false;
        for (const auto &glyph : limited.glyphs)
            ellipsis |= glyph.text == u"\u2026"_s;
        QVERIFY(ellipsis);
        for (int i = 0; i < 200; ++i)
            adapter.measure(QString::number(i), bold, subtitle, 28, 200 + i, 120, false, 1);
        QVERIFY(adapter.cacheEntries() <= 96);
        QVERIFY(adapter.cacheBytes() <= yalahoo::TextLayoutAdapter::CacheBytes);
        auto song = adapter.measure(
                u"\U0001f468\u200d\U0001f469\u200d\U0001f467\u200d\U0001f466e\u0301AV gyp Music"_s,
                bold, subtitle, 12, 48, 29, true, 1, true);
        QVERIFY(song.naturalWidth > 48);
        QVERIFY(song.fitted.endsWith(u"\u2026"_s));
        QVERIFY(!song.fitted.contains(QChar::ReplacementCharacter));
    }
    void wholeLineGlyphAssociation_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("ligatures") << u"AV office affinity e\u0301 gyp"_s;
        QTest::newRow("mixed-runs") << u"AV \u6536\u85cf office e\u0301 gyp"_s;
        QTest::newRow("bidirectional") << u"AV \u05e9\u05dc\u05d5\u05dd office"_s;
    }
    void wholeLineGlyphAssociation()
    {
        QFETCH(QString, text);
        yalahoo::TextLayoutAdapter adapter;
        auto font = yalahoo::assetFont(700);
        const auto actual = adapter.measure(text, font, font, 28, 600, 150, false, 1);
        font.setPixelSize(int(actual.size));
        font.setHintingPreference(QFont::PreferNoHinting);
        font.setLetterSpacing(QFont::AbsoluteSpacing, -.6);
        QTextLayout layout(text, font);
        layout.beginLayout();
        auto line = layout.createLine();
        line.setLineWidth(600);
        layout.endLayout();
        QList<QPair<quint32, double>> expectedInk, actualInk;
        for (const auto &run : line.glyphRuns(0, text.size(), QTextLayout::RetrieveAll))
            for (int i = 0; i < run.glyphIndexes().size(); ++i)
                expectedInk.append({ run.glyphIndexes()[i], run.positions()[i].x() });
        QString associated;
        for (const auto &glyph : actual.glyphs) {
            associated += glyph.text;
            for (const auto &run : glyph.renderRuns)
                for (int i = 0; i < run.indexes.size(); ++i)
                    actualInk.append(
                            { run.indexes[i], run.positions[i].x() * glyph.scale + glyph.x });
        }
        QCOMPARE(associated, text);
        std::sort(expectedInk.begin(), expectedInk.end());
        std::sort(actualInk.begin(), actualInk.end());
        QCOMPARE(actualInk.size(), expectedInk.size());
        for (int i = 0; i < actualInk.size(); ++i) {
            QCOMPARE(actualInk[i].first, expectedInk[i].first);
            QVERIFY(std::abs(actualInk[i].second - expectedInk[i].second) < 1e-6);
        }
        const auto wrapped =
                adapter.measure(u"AV gyp (Live) [2026]"_s, font, font, 28, 120, 150, false, 1);
        for (int i = 0; i < wrapped.glyphs.size(); ++i)
            if (i + 1 == wrapped.glyphs.size() || wrapped.glyphs[i + 1].y != wrapped.glyphs[i].y)
                QVERIFY(!wrapped.glyphs[i].text.endsWith(u' '));
        adapter.measure(text, font, font, 28, 600, 150, false, 1);
        const auto misses = adapter.misses();
        adapter.measure(text, font, font, 28, 600, 150, false, 2);
        QCOMPARE(adapter.misses(), misses + 1);
    }
    void fallbackDoesNotMovePrimaryInk()
    {
        yalahoo::TextLayoutAdapter adapter;
        const auto font = yalahoo::assetFont(700);
        for (const double dpr : { 1.0, 1.5, 2.0 }) {
            const auto plain =
                    adapter.measure(u"\u6536\u85cf"_s, font, font, 28, 600, 150, false, dpr);
            const auto mixed = adapter.measure(
                    u"\U0001f468\u200d\U0001f469\u200d\U0001f467\u200d\U0001f466 \u6536\u85cf"_s,
                    font, font, 28, 600, 150, false, dpr);
            const auto &expected = plain.glyphs.first();
            const auto found =
                    std::find_if(mixed.glyphs.cbegin(), mixed.glyphs.cend(), [](const auto &glyph) {
                        return glyph.text == u"\u6536"_s;
                    });
            QVERIFY(found != mixed.glyphs.cend());
            QCOMPARE(found->renderRuns.first().indexes, expected.renderRuns.first().indexes);
            // QTextLine positions use a 1/64-pixel fixed-point grid.
            QVERIFY(std::abs(found->renderRuns.first().positions.first().y()
                             - expected.renderRuns.first().positions.first().y())
                    <= 1.0 / 64);
        }
    }
    void macEmojiUsesSourceFace()
    {
#ifdef Q_OS_MACOS
        yalahoo::TextLayoutAdapter adapter;
        const auto primary = yalahoo::assetFont(700);
        for (const double size : { 12.0, 28.0, 29.75, 44.0 }) {
            QFont emoji(u"Apple Color Emoji"_s);
            emoji.setStyleName(u"Regular"_s);
            emoji.setWeight(QFont::Normal);
            emoji.setPixelSize(int(std::round(size)));
            emoji.setHintingPreference(QFont::PreferNoHinting);
            const auto reference = QRawFont::fromFont(emoji);
            QVERIFY(reference.isValid());
            QVERIFY(!reference.fontTable("sbix").isEmpty());
            for (const double dpr : { 1.0, 1.5, 2.0 }) {
                const auto layout = adapter.measure(
                        u"\U0001f468\u200d\U0001f469\u200d\U0001f467\u200d\U0001f466 \U0001f44d\U0001f3fd e\u0301 \U0001f1e8\U0001f1f3 \U0001f3b5 \u6536\u85cf"_s,
                        primary, primary, size, 1000, 150, false, dpr);
                int colors = 0;
                for (const auto &glyph : layout.glyphs) {
                    if (!glyph.colorGlyph)
                        continue;
                    ++colors;
                    QCOMPARE(glyph.renderRuns.size(), 1);
                    const auto &run = glyph.renderRuns.first();
                    QCOMPARE(run.font.family(), reference.familyName());
                    QCOMPARE(run.fontHead, reference.fontTable("head"));
                    QCOMPARE(run.indexes.size(), 1);
                    const double advance =
                            reference.advancesForGlyphIndexes(run.indexes).first().x();
                    QVERIFY(std::abs(glyph.width - (advance * glyph.scale - .6)) <= 1.0 / 64);
                }
                QCOMPARE(colors, 4);
            }
        }
#else
        QSKIP("Source Apple emoji face policy is macOS-specific");
#endif
    }
    void macSourceFontBoxBaseline()
    {
#ifdef Q_OS_MACOS
        // Same-host frozen CSS range top + Canvas fontBoundingBoxAscent.
        // These cover integral, fractional and translated title sizes, not GPU pixels.
        const QList<QPair<double, double>> baselines{ { 12.6, 13 }, { 19.8, 21 },
                                                      { 20, 21 },   { 24.609375, 26 },
                                                      { 28, 30 },   { 29.75, 32 },
                                                      { 44, 47 } };
        yalahoo::TextLayoutAdapter adapter;
        const auto font = yalahoo::assetFont(700);
        for (const double dpr : { 1.0, 1.5, 2.0 }) {
            for (const auto &[size, baseline] : baselines) {
                const auto layout =
                        adapter.measure(u"\u6536\u85cf"_s, font, font, size, 600, 150, false, dpr);
                QCOMPARE(layout.glyphs.size(), 2);
                for (const auto &glyph : layout.glyphs) {
                    const auto &run = glyph.renderRuns.first();
                    QVERIFY(std::abs(run.positions.first().y() * glyph.scale - baseline)
                            <= 1.0 / 64);
                }
            }
        }
        const auto translated =
                adapter.measure(u"\u97f3\u4e50\uff08Music archive gyp\uff09"_s, font,
                                yalahoo::assetFont(600), 44, 500, 220, false, 2);
        for (const auto &glyph : translated.glyphs) {
            if (!glyph.translation)
                continue;
            QVERIFY(std::abs(glyph.renderRuns.first().positions.first().y() * glyph.scale - 21)
                    <= 1.0 / 64);
        }
#else
        QSKIP("Frozen CoreText font-box alignment is macOS-specific");
#endif
    }
    void analyticAndInterruptions()
    {
        for (const int fps : { 20, 30, 60, 120 }) {
            yalahoo::ReelTrack track;
            track.move(0, true);
            track.move(60);
            for (int i = 0; i < fps / 4; ++i) {
                track.advance(1.0 / fps);
                const double t = track.elapsed / .460;
                const double value = 60 + (-60 - 600 * t) * std::exp(-10 * t);
                const double velocity = 6000 * t * std::exp(-10 * t) / .460;
                QVERIFY(std::abs(track.sample().value - value) <= 1e-6);
                QVERIFY(std::abs(track.sample().velocity - velocity) <= 1e-6);
            }
            auto before = track.sample();
            track.move(-30);
            QVERIFY(std::abs(track.sample().value - before.value) <= 1e-6);
            QVERIFY(std::abs(track.sample().velocity - before.velocity) <= 1e-6);
            auto elapsed = track.elapsed;
            track.move(-30);
            QCOMPARE(track.elapsed, elapsed);
            track.advance(.460);
            QCOMPARE(track.sample().value, -30.0);
            QCOMPARE(track.sample().velocity, 0.0);
        }
    }
    void naturalWidthDoesNotRetargetDuringVisibilityChanges()
    {
        yalahoo::TitleReel reel;
        reel.setFont(yalahoo::assetFont(400));
        reel.setTransport(true);
        reel.setText(u"\u6b4c\u66f2\u6807\u9898"_s);
        reel.componentComplete();
        QSignalSpy widths(&reel, &yalahoo::TitleReel::naturalWidthChanged);
        // Model the transport's visibility change while its width binding is evaluated.
        connect(&reel, &yalahoo::TitleReel::naturalWidthChanged, &reel, [&] {
            reel.setActive(false);
        });
        reel.setText(u"\u66f4\u957f\u7684\u6b4c\u66f2\u6807\u9898"_s);
        reel.reconcile();
        QCOMPARE(widths.count(), 1);
        const double width = reel.naturalWidth();
        reel.setActive(true);
        reel.finish();
        reel.advance(.05);
        QCOMPARE(reel.naturalWidth(), width);
        QCOMPARE(widths.count(), 1);
    }
    void hiddenFinishIsDeferredAndCoalesced()
    {
        yalahoo::TitleReel reel;
        reel.setFont(yalahoo::assetFont(400));
        reel.setText(u"\u7b2c\u4e00\u9996\u6b4c\u66f2"_s);
        reel.componentComplete();
        reel.setText(u"\u4e0b\u4e00\u9996\u6b4c\u66f2"_s);
        reel.reconcile();
        QVERIFY(reel.moving());
        QSignalSpy frames(&reel, &yalahoo::TitleReel::changed);
        for (int i = 0; i < 10; ++i) {
            reel.setActive(false);
            reel.setActive(true);
        }
        reel.setActive(false);
        QCOMPARE(frames.count(), 0);
        QCoreApplication::sendPostedEvents(&reel, QEvent::MetaCall);
        QCOMPARE(frames.count(), 1);
        QVERIFY(!reel.moving());
        // A restored item must retain its motion when an old hide callback arrives.
        reel.setActive(true);
        reel.setText(u"\u6062\u590d\u7684\u6b4c\u66f2"_s);
        reel.reconcile();
        QVERIFY(reel.moving());
        reel.setActive(false);
        reel.setActive(true);
        QCoreApplication::sendPostedEvents(&reel, QEvent::MetaCall);
        QVERIFY(reel.moving());
    }
    void boundedPoolAndSingleClock()
    {
        yalahoo::FrameClock clock;
        clock.setActive(true);
        clock.advance(0);
        yalahoo::TitleReel reel;
        QAbstractItemModelTester modelCheck(&reel,
                                            QAbstractItemModelTester::FailureReportingMode::QtTest);
        reel.setClock(&clock);
        reel.setFont(yalahoo::assetFont(700));
        reel.setSubtitleFont(yalahoo::assetFont(600));
        reel.setText(u"\u6536\u85cf\u7684\u58f0\u97f3\uff08Sound archive\uff09"_s);
        reel.componentComplete();
        QVERIFY(!reel.moving());
        auto *slot = reel.slotAt(0);
        reel.setText(u"\u6628\u65e5\u7684\u6545\u4e8b\uff08Stories gyp\uff09"_s);
        reel.reconcile();
        QCOMPARE(reel.slotAt(0), slot);
        QVERIFY(reel.moving());
        const auto misses = reel.diagnostics()[u"layoutMisses"_s];
        clock.setSpeed(.25);
        clock.advance(.05);
        QCOMPARE(slot->roll.elapsed, .0125);
        clock.setSpeed(3);
        clock.advance(.05);
        QCOMPARE(slot->roll.elapsed, .1625);
        QCOMPARE(reel.diagnostics()[u"layoutMisses"_s], misses);
        for (int i = 0; i < 100; ++i) {
            reel.setText(
                    u"\u97f3\u4e50 %1 \U0001f468\u200d\U0001f469\u200d\U0001f467\u200d\U0001f466 e\u0301"_s
                            .arg(i));
            reel.reconcile();
            clock.advance(1.0 / 120);
            QVERIFY(reel.diagnostics()[u"faces"_s].toInt() <= reel.slotCount() * 8);
        }
        reel.setReducedMotion(true);
        QVERIFY(!reel.moving());
        QVERIFY(reel.diagnostics()[u"slots"_s].toInt()
                        - reel.diagnostics()[u"activeSlots"_s].toInt()
                <= 64);
        reel.setReducedMotion(false);
        reel.setText(u"\u66f4\u591a\u7684\u6536\u85cf"_s);
        reel.reconcile();
        reel.setAvailableWidth(90);
        reel.reconcile();
        QVERIFY(!reel.moving());
        reel.setActive(false);
        const double height = reel.layoutHeight();
        clock.advance(.05);
        QCOMPARE(reel.layoutHeight(), height);
        const auto beforeInvalidation = reel.diagnostics()[u"layoutMisses"_s].toULongLong();
        QVERIFY(QMetaObject::invokeMethod(qGuiApp, "fontDatabaseChanged"));
        reel.reconcile();
        QVERIFY(reel.diagnostics()[u"layoutMisses"_s].toULongLong() > beforeInvalidation);
    }
    void transportTiming()
    {
        yalahoo::TransportMotion motion;
        motion.componentComplete();
        motion.setTargetWidth(200);
        motion.setShown(true);
        QCOMPARE(motion.width(), 0.0);
        QCOMPARE(motion.offset(), 18.0);
        motion.advance(.180);
        const double width = motion.width();
        QVERIFY(width > 0 && width < 200);
        motion.setPlaying(true);
        motion.advance(.180);
        QCOMPARE(motion.pauseOpacity(), 1.0);
        motion.advance(.040);
        QCOMPARE(motion.opacity(), 1.0);
        motion.advance(.240);
        QCOMPARE(motion.width(), 200.0);
        QCOMPARE(motion.offset(), 0.0);
        motion.setShown(false);
        motion.advance(.100);
        const double before = motion.width();
        motion.setShown(true);
        QCOMPARE(motion.width(), before);
        motion.setReducedMotion(true);
        QCOMPARE(motion.width(), 200.0);
        QVERIFY(!motion.moving());
    }
};
QTEST_MAIN(TitleTest)
#include "TitleTest.moc"
