// Verifies whole-run shaping, interrupted slot motion and visibility/cache lifecycle on
// anonymous text. Explicit fixture literals are independent inputs or expected results; timing
// bounds have named constants.

#include "FrameClock.h"
#include "TextLayoutAdapter.h"
#include "TitleReel.h"
#include "ReelInk.h"
#include <QQuickRenderControl>
#include <QQuickWindow>
#include <QSGNode>
#include <QScopeGuard>
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
#include <utility>

using namespace Qt::StringLiterals;

class TitleTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TitleTest)
public:
    TitleTest() = default;

private Q_SLOTS:
    void modelObserversCannotReconcileInsideInsertOrRemoval()
    {
        yalahoo::TitleReel reel;
        reel.setFont(yalahoo::assetFont(400));
        reel.setTransport(true);
        reel.setAvailableWidth(2000);
        reel.setBudget(200);
        reel.setReducedMotion(true);
        reel.componentComplete();
        QAbstractItemModelTester tester(&reel,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        bool inserted = false, removed = false;
        connect(&reel, &QAbstractItemModel::rowsInserted, &reel, [&] {
            if (!std::exchange(inserted, true)) {
                reel.setText(u"Latest title"_s);
                reel.reconcile();
                reel.finish();
                reel.advance(.1);
            }
        });
        reel.setText(u"Original"_s);
        reel.reconcile();
        QTRY_COMPARE(reel.fittedText(), u"Latest title"_s);
        const auto activeText = [&] {
            QString result;
            for (int i = 0; i < reel.slotCount(); ++i)
                if (reel.slotAt(i)->active)
                    result += reel.slotAt(i)->glyph.text;
            return result;
        };
        QCOMPARE(activeText(), u"Latest title"_s);
        reel.setText(QString(200, u'X'));
        reel.reconcile();
        connect(&reel, &QAbstractItemModel::rowsRemoved, &reel, [&] {
            if (!std::exchange(removed, true)) {
                reel.setText(u"Final"_s);
                reel.reconcile();
                reel.finish();
            }
        });
        reel.setText(u"A"_s);
        reel.reconcile();
        QTRY_VERIFY(removed);
        QTRY_COMPARE(reel.fittedText(), u"Final"_s);
        QCOMPARE(activeText(), u"Final"_s);
    }

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
    void transportElidesBidirectionalGraphemes_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("hebrew") << u"\u05e9\u05dc\u05d5\u05dd \u05e2\u05d5\u05dc\u05dd "_s.repeated(
                10);
        QTest::newRow("arabic")
                << u"\u0645\u0631\u062d\u0628\u0627 \u0628\u0627\u0644\u0639\u0627\u0644\u0645 "_s
                           .repeated(10);
        QTest::newRow("mixed-rtl-first")
                << u"\u05e9\u05dc\u05d5\u05dd Archive e\u0301 "_s.repeated(10);
        QTest::newRow("mixed-ltr-first")
                << u"Archive \u0645\u0631\u062d\u0628\u0627 e\u0301 "_s.repeated(10);
        QTest::newRow("emoji")
                << u"\U0001f468\u200d\U0001f469\u200d\U0001f467\u200d\U0001f466e\u0301 Music "_s
                           .repeated(10);
    }
    void transportElidesBidirectionalGraphemes()
    {
        QFETCH(QString, text);
        yalahoo::TextLayoutAdapter adapter;
        const auto font = yalahoo::assetFont(400);
        for (const double size : { 12., 12.5 }) {
            for (const double width : { 0., 2., 80., 240. }) {
                const auto result =
                        adapter.measure(text, font, font, size, width, 29, true, 1, true);
                QVERIFY(result.naturalWidth > width);
                const auto fitted =
                        adapter.measure(result.fitted, font, font, size, 10000, 29, true, 1, true);
                QVERIFY2(fitted.naturalWidth <= width + 1e-6, qPrintable(result.fitted));
                if (width >= 80) {
                    QVERIFY(result.glyphs.size() > 1);
                    QVERIFY(result.fitted.contains(u'\u2026'));
                    QVERIFY(result.fitted.size() < text.size());
                } else {
                    QVERIFY(result.fitted.isEmpty());
                }
                QVERIFY(!result.fitted.contains(QChar::ReplacementCharacter));
            }
        }
        const auto shortText = text.first(yalahoo::TextLayoutAdapter::boundaries(text)[1]);
        QCOMPARE(adapter.measure(shortText, font, font, 12, 80, 29, true, 1, true).fitted,
                 shortText);
    }
    void mainTitleElidesBidirectionalGraphemes_data()
    {
        transportElidesBidirectionalGraphemes_data();
    }
    void mainTitleElidesBidirectionalGraphemes()
    {
        QFETCH(QString, text);
        yalahoo::TextLayoutAdapter adapter;
        const auto font = yalahoo::assetFont(700);
        for (const double width : { 80., 150., 240. }) {
            const auto result =
                    adapter.measure(text.repeated(10), font, font, 28, width, 65, false, 1);
            QVERIFY(result.overflow);
            const auto dot = std::find_if(result.glyphs.cbegin(), result.glyphs.cend(),
                                          [](const auto &glyph) {
                                              return glyph.text.contains(u'\u2026');
                                          });
            QVERIFY(dot != result.glyphs.cend());
            QVERIFY(dot->y > 0);
            for (const auto &glyph : result.glyphs) {
                QVERIFY(!glyph.text.contains(QChar::ReplacementCharacter));
                if (glyph.y != dot->y)
                    continue;
                QVERIFY2(glyph.x >= -1e-6 && glyph.x + glyph.width <= width + 1e-6,
                         qPrintable(glyph.text));
                if (&glyph != &*dot)
                    QVERIFY2(glyph.x + glyph.width <= dot->x + 1e-6
                                     || glyph.x >= dot->x + dot->width - 1e-6,
                             qPrintable(glyph.text));
            }
        }
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
        QList<std::pair<quint32, double>> expectedInk, actualInk;
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
    void supplementaryTextKeepsRequestedFont()
    {
        yalahoo::TextLayoutAdapter adapter;
        QFont font;
#ifdef Q_OS_MACOS
        font.setFamily(u"Songti SC"_s);
#endif
        font.setWeight(QFont::Bold);
        font.setPixelSize(28);
        font.setHintingPreference(QFont::PreferNoHinting);
        const auto text = u"\U00020000"_s;
        QTextLayout reference(text, font);
        reference.beginLayout();
        reference.createLine().setLineWidth(600);
        reference.endLayout();
        const auto expected = reference.glyphRuns();
        QVERIFY(!expected.isEmpty());
        const auto actual = adapter.measure(text, font, font, 28, 600, 150, false, 1);
        QVERIFY(!actual.glyphs.isEmpty());
        const auto &runs = actual.glyphs.first().renderRuns;
        QVERIFY(!runs.isEmpty());
        QCOMPARE(runs.first().font.weight(), expected.first().rawFont().weight());
        QCOMPARE(runs.first().fontHead, expected.first().rawFont().fontTable("head"));
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
        const QList<std::pair<double, double>> baselines{ { 12.6, 13 }, { 19.8, 21 },
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
    void sampledRetargetsPreserveTheAnalyticGrid()
    {
        // Different glyphs and durations reuse the grid, never another track's state.
        for (const double duration : { .299, .460, .7 }) {
            for (const double velocity : { -180., 0., 73. }) {
                yalahoo::sampledReel::Points points;
                yalahoo::sampledReel::fill(points, -12, 31, velocity, duration);
                for (int i = 0; i < 48; ++i) {
                    const double t = double(i) / 48;
                    const double expected =
                            31 + (-43 + (velocity * duration - 430) * t) * std::exp(-10 * t);
                    QVERIFY(std::abs(points[i] - expected) < 1e-12);
                }
                QCOMPARE(points[48], 31.);
            }
        }
    }
    void identicalFacesDoNotRepublishContent()
    {
        yalahoo::ReelSlot slot;
        yalahoo::TitleGlyph glyph;
        glyph.text = u"A"_s;
        glyph.font = yalahoo::assetFont(400);
        glyph.size = 24;
        glyph.width = 18;
        glyph.height = 32;
        slot.glyph = glyph;
        QSignalSpy contents(&slot, &yalahoo::ReelSlot::contentChanged);
        slot.writeFaces({ glyph });
        QCOMPARE(contents.count(), 1);
        const auto revision = slot.revision;
        const auto values = slot.faceValues();
        slot.writeFaces({ glyph });
        slot.rollTo(glyph, true);
        QCOMPARE(contents.count(), 1);
        QCOMPARE(slot.revision, revision);
        QCOMPARE(slot.faceValues(), values);
        // Full descriptor changes must still invalidate cached descriptors and native ink.
        glyph.baseline += .5;
        slot.writeFaces({ glyph });
        QCOMPARE(contents.count(), 2);
        QCOMPARE(slot.faces.first().glyph.baseline, .5);
        yalahoo::TitleGlyph empty;
        slot.writeFaces({ empty });
        QCOMPARE(slot.faces.first().glyph.height, 32.);
        const auto blankRevision = slot.revision;
        slot.writeFaces({ empty });
        QCOMPARE(slot.revision, blankRevision);
        slot.glyph.size = 12;
        slot.glyph.translation = true;
        slot.writeFaces({ empty });
        QCOMPARE(slot.revision, blankRevision + 1);
        QVERIFY(slot.translation());
        QCOMPARE(slot.pixelSize(), 12.);
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
    void longRetiredSuffixUsesOneModelTransaction()
    {
        yalahoo::TitleReel reel;
        QAbstractItemModelTester tester(&reel,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        reel.setTransport(true);
        reel.setReducedMotion(true);
        reel.setFont(yalahoo::assetFont(700));
        reel.setAvailableWidth(10000);
        reel.setText(QString(256, u'W'));
        reel.componentComplete();
        QCOMPARE(reel.slotCount(), 256);
        auto *first = reel.slotAt(0);
        QSignalSpy removed(&reel, &QAbstractItemModel::rowsRemoved);
        QSignalSpy reset(&reel, &QAbstractItemModel::modelReset);
        reel.setText(u"W"_s);
        reel.reconcile();
        QCOMPARE(reel.slotAt(0), first);
        QCOMPARE(reel.diagnostics().value(u"activeSlots"_s).toInt(), 1);
        QCOMPARE(reel.slotCount(), 65);
        QCOMPARE(removed.size(), 1);
        QCOMPARE(reset.size(), 0);
        reel.setText(QString(256, u'W'));
        reel.reconcile();
        QCOMPARE(reel.slotAt(0), first);
        QCOMPARE(reel.slotCount(), 256);
        for (int i = 0; i < reel.slotCount(); ++i) {
            QVERIFY(reel.slotAt(i)->active);
            QCOMPARE(reel.slotAt(i)->glyph.text, u"W"_s);
        }
    }
    void reboundSlotsWithEqualRevisionsRefreshInk()
    {
        // A hidden null-RHI scene graph exercises the production node cache without
        // requiring a native surface or a GPU submission.
        const auto api = QQuickWindow::graphicsApi();
        const auto restoreApi = qScopeGuard([api] {
            QQuickWindow::setGraphicsApi(api);
        });
        QQuickWindow::setSceneGraphBackend(u"rhi"_s);
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Null);
        QQuickRenderControl control;
        QQuickWindow window(&control);
        QVERIFY(control.initialize());
        struct InkHarness final : yalahoo::ReelInk
        {
            QSGNode *synchronize(QSGNode *previous)
            {
                return updatePaintNode(previous, nullptr);
            }
        } ink;
        ink.setParentItem(window.contentItem());
        yalahoo::TextLayoutAdapter layout;
        const auto font = yalahoo::assetFont(700);
        const auto a = layout.measure(u"A"_s, font, font, 28, 100, 100, false, 1).glyphs.first();
        const auto b = layout.measure(u"B"_s, font, font, 28, 100, 100, false, 1).glyphs.first();
        yalahoo::ReelSlot first;
        first.active = true;
        first.glyph = a;
        first.writeFaces({ a });
        auto second = std::make_unique<yalahoo::ReelSlot>();
        second->active = true;
        second->glyph = b;
        second->writeFaces({ b });
        QCOMPARE(first.revision, second->revision);
        ink.setSlot(&first);
        auto node = std::unique_ptr<QSGNode>(ink.synchronize(nullptr));
        QVERIFY(node);
        QCOMPARE(node->childCount(), 1);
        const auto updates = yalahoo::ReelInk::geometryUpdates();
        ink.setSlot(second.get());
        node.reset(ink.synchronize(node.release()));
        QCOMPARE(yalahoo::ReelInk::geometryUpdates(), updates + 1);
        QCOMPARE(node->childCount(), 1);
        const auto reused = yalahoo::ReelInk::geometryReuses();
        yalahoo::ReelSlot identical;
        identical.active = true;
        identical.glyph = b;
        identical.writeFaces({ b });
        ink.setSlot(&identical);
        node.reset(ink.synchronize(node.release()));
        QCOMPARE(yalahoo::ReelInk::geometryUpdates(), updates + 1);
        QCOMPARE(yalahoo::ReelInk::geometryReuses(), reused + 1);
        ink.setSlot(second.get());
        node.reset(ink.synchronize(node.release()));
        QSignalSpy changed(&ink, &yalahoo::ReelInk::slotChanged);
        second.reset();
        QCOMPARE(changed.size(), 1);
        QVERIFY(!ink.slot());
        node.reset(ink.synchronize(node.release()));
        QVERIFY(!node);
        ink.setParentItem(nullptr);
        control.invalidate();
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
