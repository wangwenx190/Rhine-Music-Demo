#include "TextLayoutAdapter.h"
#include "TitleReel.h"
#include "TransportMotion.h"
#include "AssetCatalog.h"
#include <QTest>
#include <QFontMetricsF>
#include <QAbstractItemModelTester>
#include <QTextLayout>
#include <algorithm>
#include <cmath>

class TitleTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QVERIFY(yalahoo::initializeAssetFonts());
    }
    void graphemesAndParts()
    {
        const QString text = QStringLiteral("A👨‍👩‍👧‍👦👍🏽é音乐（gyp）");
        const auto boundaries = yalahoo::TextLayoutAdapter::boundaries(text);
        QCOMPARE(boundaries.size(), 12);
        QCOMPARE(text.mid(boundaries[1], boundaries[2] - boundaries[1]),
                 QStringLiteral("👨‍👩‍👧‍👦"));
        QCOMPARE(text.mid(boundaries[2], boundaries[3] - boundaries[2]), QStringLiteral("👍🏽"));
        QCOMPARE(text.mid(boundaries[3], boundaries[4] - boundaries[3]), QStringLiteral("é"));
        auto parts =
                yalahoo::TextLayoutAdapter::titleParts(QStringLiteral("音乐收藏（Archive gyp）"));
        QCOMPARE(parts.first, QStringLiteral("音乐收藏"));
        QCOMPARE(parts.second, QStringLiteral("Archive gyp"));
        QCOMPARE(yalahoo::TextLayoutAdapter::titleParts(QStringLiteral("Music (Live)")).second,
                 QString());
        QCOMPARE(yalahoo::TextLayoutAdapter::titleParts(QStringLiteral("音乐（现场版）")).second,
                 QString());
    }
    void layoutAndCache()
    {
        yalahoo::TextLayoutAdapter adapter;
        const auto bold = yalahoo::assetFont(700), subtitle = yalahoo::assetFont(600);
        auto a = adapter.measure(QStringLiteral("音乐收藏（Archive gyp）"), bold, subtitle, 28, 270,
                                 180, false, 1);
        QVERIFY(a.height > 28 * 1.4);
        QVERIFY(a.glyphs.last().translation);
        const auto misses = adapter.misses();
        auto b = adapter.measure(QStringLiteral("音乐收藏（Archive gyp）"), bold, subtitle, 28, 270,
                                 180, false, 1);
        QCOMPARE(a.glyphs.size(), b.glyphs.size());
        QCOMPARE(adapter.misses(), misses);
        for (const auto &glyph : a.glyphs) {
            QVERIFY(glyph.height >= glyph.size * 1.4);
            QVERIFY(glyph.width >= 0);
            QVERIFY(!glyph.renderRuns.isEmpty());
        }
        const auto longTitle =
                QStringLiteral("收藏的音乐声音和往日的故事（The collected archive of music gyp）")
                        .repeated(20);
        auto limited = adapter.measure(longTitle, bold, subtitle, 28, 150, 65, false, 2);
        QVERIFY(limited.overflow);
        QCOMPARE(limited.size, 20.0);
        QCOMPARE(limited.height, 65.0);
        bool ellipsis = false;
        for (const auto &glyph : limited.glyphs)
            ellipsis |= glyph.text == QStringLiteral("…");
        QVERIFY(ellipsis);
        for (int i = 0; i < 200; ++i)
            adapter.measure(QString::number(i), bold, subtitle, 28, 200 + i, 120, false, 1);
        QVERIFY(adapter.cacheEntries() <= 96);
        QVERIFY(adapter.cacheBytes() <= yalahoo::TextLayoutAdapter::CacheBytes);
        auto song = adapter.measure(QStringLiteral("👨‍👩‍👧‍👦éAV gyp Music"),
                                    bold, subtitle, 12, 48, 29, true, 1, true);
        QVERIFY(song.naturalWidth > 48);
        QVERIFY(song.fitted.endsWith(QStringLiteral("…")));
        QVERIFY(!song.fitted.contains(QChar::ReplacementCharacter));
    }
    void wholeLineGlyphAssociation()
    {
        yalahoo::TextLayoutAdapter adapter;
        auto font = yalahoo::assetFont(700);
        const QString text = QStringLiteral("AV office affinity é gyp");
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
        const auto wrapped = adapter.measure(QStringLiteral("AV gyp (Live) [2026]"), font, font, 28,
                                             120, 150, false, 1);
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
                    adapter.measure(QStringLiteral("收藏"), font, font, 28, 600, 150, false, dpr);
            const auto mixed = adapter.measure(QStringLiteral("👨‍👩‍👧‍👦 收藏"),
                                               font, font, 28, 600, 150, false, dpr);
            const auto &expected = plain.glyphs.first();
            const auto found =
                    std::find_if(mixed.glyphs.cbegin(), mixed.glyphs.cend(), [](const auto &glyph) {
                        return glyph.text == QStringLiteral("收");
                    });
            QVERIFY(found != mixed.glyphs.cend());
            QCOMPARE(found->renderRuns.first().indexes, expected.renderRuns.first().indexes);
            // QTextLine positions use a 1/64-pixel fixed-point grid.
            QVERIFY(std::abs(found->renderRuns.first().positions.first().y()
                             - expected.renderRuns.first().positions.first().y())
                    <= 1.0 / 64);
        }
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
        reel.setText(QStringLiteral("收藏的声音（Sound archive）"));
        reel.componentComplete();
        QVERIFY(!reel.moving());
        auto *slot = reel.slotAt(0);
        reel.setText(QStringLiteral("昨日的故事（Stories gyp）"));
        reel.reconcile();
        QCOMPARE(reel.slotAt(0), slot);
        QVERIFY(reel.moving());
        const auto misses = reel.diagnostics()["layoutMisses"];
        clock.setSpeed(.25);
        clock.advance(.05);
        QCOMPARE(slot->roll.elapsed, .0125);
        clock.setSpeed(3);
        clock.advance(.05);
        QCOMPARE(slot->roll.elapsed, .1625);
        QCOMPARE(reel.diagnostics()["layoutMisses"], misses);
        for (int i = 0; i < 100; ++i) {
            reel.setText(QStringLiteral("音乐 %1 👨‍👩‍👧‍👦 é").arg(i));
            reel.reconcile();
            clock.advance(1.0 / 120);
            QVERIFY(reel.diagnostics()["faces"].toInt() <= reel.slotCount() * 8);
        }
        reel.setReducedMotion(true);
        QVERIFY(!reel.moving());
        QVERIFY(reel.diagnostics()["slots"].toInt() - reel.diagnostics()["activeSlots"].toInt()
                <= 64);
        reel.setReducedMotion(false);
        reel.setText(QStringLiteral("更多的收藏"));
        reel.reconcile();
        reel.setAvailableWidth(90);
        reel.reconcile();
        QVERIFY(!reel.moving());
        reel.setActive(false);
        const double height = reel.layoutHeight();
        clock.advance(.05);
        QCOMPARE(reel.layoutHeight(), height);
        const auto beforeInvalidation = reel.diagnostics()["layoutMisses"].toULongLong();
        QVERIFY(QMetaObject::invokeMethod(qGuiApp, "fontDatabaseChanged"));
        reel.reconcile();
        QVERIFY(reel.diagnostics()["layoutMisses"].toULongLong() > beforeInvalidation);
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
