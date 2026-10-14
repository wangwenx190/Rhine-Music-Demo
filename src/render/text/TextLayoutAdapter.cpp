#include "TextLayoutAdapter.h"
#include <QTextLayout>
#include <QTextBoundaryFinder>
#include <QFontMetricsF>
#include <QRegularExpression>
#include <QDataStream>
#include <QIODevice>
#include <QMap>
#include <algorithm>
#include <cmath>

namespace yalahoo {
namespace {
struct Part
{
    QList<TitleGlyph> glyphs;
    double height = 0, naturalWidth = 0;
    int lines = 0;
};
Part shape(const QString &text, QFont font, double size, double width, double ratio, double spacing,
           bool translation, bool oneLine, bool clamp, bool collect = true, double budget = 300)
{
    Part part;
    if (text.isEmpty())
        return part;
    font.setPixelSize(std::max(1, int(std::round(size))));
    const double scale = size / font.pixelSize();
    font.setHintingPreference(QFont::PreferNoHinting);
    font.setLetterSpacing(QFont::AbsoluteSpacing, spacing / scale);
    QTextLayout layout(text, font);
    const auto graphemes = TextLayoutAdapter::boundaries(text);
    QList<QTextLayout::FormatRange> formats;
    for (int i = 0; i + 1 < graphemes.size(); ++i) {
        const auto cluster = text.mid(graphemes[i], graphemes[i + 1] - graphemes[i]);
        bool emoji = false;
        for (const auto code : cluster.toUcs4())
            emoji |= code >= 0x1f000 || code == 0xfe0f;
        if (emoji) {
            QTextCharFormat format;
            // Frozen CSS font-synthesis:none keeps a regular color-font fallback regular.
            // The embedded primary faces still use their exact original SFNT weight/style.
            format.setFontWeight(QFont::Normal);
            format.setFontStyleName(QStringLiteral("Regular"));
            formats.append({ graphemes[i], graphemes[i + 1] - graphemes[i], format });
        }
    }
    layout.setFormats(formats);
    QTextOption option;
    option.setWrapMode(oneLine ? QTextOption::NoWrap : QTextOption::WrapAtWordBoundaryOrAnywhere);
    option.setFlags(QTextOption::IncludeTrailingSpaces);
    layout.setTextOption(option);
    layout.setCacheEnabled(true);
    const double lineHeight = oneLine && ratio == 0 ? 29 : size * ratio;
    layout.beginLayout();
    const int limit = oneLine ? 1 : translation ? int(std::ceil(budget / (size * ratio))) + 1 : 3;
    for (int index = 0; index < limit; ++index) {
        auto line = layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(oneLine ? 1e9 : width / scale);
        // CSS inline line box: independently rounded font ascent/descent, plus half leading.
        const QFontMetricsF metrics(font);
        const double ascent = metrics.ascent() * scale;
        const double descent = metrics.descent() * scale;
        const double top = (lineHeight - std::round(ascent) - std::round(descent)) / 2
                + std::round(ascent) - ascent;
        line.setPosition({ 0, (index * lineHeight + top) / scale });
        part.naturalWidth = std::max(part.naturalWidth, line.naturalTextWidth() * scale);
        ++part.lines;
    }
    layout.endLayout();
    const int visibleLines = oneLine ? 1 : clamp ? std::min(part.lines, 2) : part.lines;
    part.height = visibleLines * lineHeight;
    if (!collect)
        return part;
    for (int index = 0; index < visibleLines; ++index) {
        auto line = layout.lineAt(index);
        const int lineStart = line.textStart();
        const int end = lineStart + line.textLength();
        int inkEnd = end;
        if (!oneLine)
            while (inkEnd > lineStart && text[inkEnd - 1] == u' ')
                --inkEnd;
        const auto runs = line.glyphRuns(lineStart, line.textLength(), QTextLayout::RetrieveAll);
        QList<ResolvedGlyphRun> resolvedRuns;
        QList<bool> colorRuns;
        QMap<int, QList<QPair<int, int>>> groups;
        for (int runIndex = 0; runIndex < runs.size(); ++runIndex) {
            const auto &run = runs[runIndex];
            const auto raw = run.rawFont();
            QFont resolved(raw.familyName());
            resolved.setStyleName(raw.styleName());
            resolved.setWeight(QFont::Weight(raw.weight()));
            resolved.setPixelSize(int(std::round(raw.pixelSize())));
            resolved.setHintingPreference(QFont::PreferNoHinting);
            resolvedRuns.append({ resolved, raw.fontTable("head"), run.glyphIndexes(),
                                  run.positions(), run.flags() });
            colorRuns.append(!raw.fontTable("COLR").isEmpty() || !raw.fontTable("CBDT").isEmpty()
                             || !raw.fontTable("sbix").isEmpty());
            const auto strings = run.stringIndexes();
            for (int i = 0; i < strings.size(); ++i) {
                const auto boundary =
                        std::upper_bound(graphemes.begin(), graphemes.end(), int(strings[i]));
                const int start = boundary == graphemes.begin()
                        ? lineStart
                        : std::max(lineStart, *(boundary - 1));
                groups[start].append({ runIndex, i });
            }
        }
        int column = 0;
        for (auto group = groups.cbegin(); group != groups.cend(); ++group) {
            const int start = group.key();
            const auto following = std::next(group);
            const int next = following == groups.cend() ? end : following.key();
            // Cluster starts from the entire shaped line merge ligatures without re-querying
            // every character range or changing contextual glyph forms.
            TitleGlyph glyph;
            if (!oneLine && start >= inkEnd)
                continue;
            glyph.key = QString("%1:%2:%3")
                                .arg(translation ? "translation" : "main")
                                .arg(index)
                                .arg(column++);
            glyph.text = text.mid(start, std::min(next, inkEnd) - start);
            glyph.translation = translation;
            const double a = line.cursorToX(start) * scale;
            const double b = line.cursorToX(next) * scale;
            glyph.x = std::min(a, b);
            glyph.y = index * lineHeight;
            glyph.width = std::abs(b - a);
            glyph.height = lineHeight;
            glyph.size = size;
            glyph.scale = scale;
            glyph.font = font;
            glyph.baseline = line.ascent() + line.y() - glyph.y / scale;
            bool colored = false;
            QMap<int, QList<int>> runGroups;
            for (const auto &value : group.value())
                runGroups[value.first].append(value.second);
            for (auto runGroup = runGroups.cbegin(); runGroup != runGroups.cend(); ++runGroup) {
                const auto &source = resolvedRuns[runGroup.key()];
                ResolvedGlyphRun run{ source.font, source.fontHead, { }, { }, source.flags };
                for (const int i : runGroup.value()) {
                    run.indexes.append(source.indexes[i]);
                    run.positions.append(source.positions[i]
                                         - QPointF(glyph.x / scale, glyph.y / scale));
                }
                glyph.renderRuns.append(run);
                colored |= colorRuns[runGroup.key()];
            }
            if (colored) {
                glyph.colorGlyph = true;
                glyph.font.setWeight(QFont::Normal);
                glyph.font.setStyleName(QStringLiteral("Regular"));
            }
            part.glyphs.append(glyph);
        }
        if (clamp && part.lines > visibleLines && index == visibleLines - 1) {
            const auto ellipsis = shape(QStringLiteral("…"), font, size, width, ratio, spacing,
                                        translation, true, false)
                                          .glyphs.first();
            while (!part.glyphs.isEmpty() && part.glyphs.last().y == index * lineHeight
                   && part.glyphs.last().x + part.glyphs.last().width + ellipsis.width > width)
                part.glyphs.removeLast();
            TitleGlyph dot = ellipsis;
            dot.key = QString("%1:%2:%3")
                              .arg(translation ? "translation" : "main")
                              .arg(index)
                              .arg(column);
            dot.x = !part.glyphs.isEmpty() && part.glyphs.last().y == index * lineHeight
                    ? part.glyphs.last().x + part.glyphs.last().width
                    : 0;
            dot.y = index * lineHeight;
            dot.height = lineHeight;
            part.glyphs.append(dot);
        }
    }
    return part;
}
}
TextLayoutAdapter::TextLayoutAdapter() : m_cache(CacheBytes)
{
}
QList<int> TextLayoutAdapter::boundaries(const QString &text)
{
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
    QList<int> result{ 0 };
    for (int position = finder.toNextBoundary(); position >= 0; position = finder.toNextBoundary())
        result.append(position);
    return result;
}
QPair<QString, QString> TextLayoutAdapter::titleParts(const QString &text)
{
    static const QRegularExpression pattern(QStringLiteral("^(.+?)[（(]([^（）()]+)[）)]\\s*$"));
    static const QRegularExpression han(QStringLiteral("\\p{Han}"));
    static const QRegularExpression latin(QStringLiteral("[a-z]"),
                                          QRegularExpression::CaseInsensitiveOption);
    const auto match = pattern.match(text.trimmed());
    if (match.hasMatch() && match.captured(1).contains(han) && match.captured(2).contains(latin)
        && !match.captured(2).contains(han))
        return { match.captured(1).trimmed(), match.captured(2).trimmed() };
    return { text, { } };
}
TitleLayout TextLayoutAdapter::measure(const QString &text, const QFont &main,
                                       const QFont &subtitle, double size, double width,
                                       double budget, bool narrow, double dpr, bool transport)
{
    QByteArray keyBytes;
    QDataStream stream(&keyBytes, QIODevice::WriteOnly);
    stream << text << main << subtitle << size << width << budget << narrow << dpr << transport;
    const QString key = QString::fromLatin1(keyBytes.toBase64());
    if (auto *cached = m_cache.object(key)) {
        m_keys.removeAll(key);
        m_keys.append(key);
        return *cached;
    }
    ++m_misses;
    TitleLayout result;
    auto parts = transport ? qMakePair(text, QString()) : titleParts(text);
    if (!transport) {
        static const QRegularExpression whitespace(QStringLiteral("[\\t\\n\\r\\f ]+"));
        static const QRegularExpression ends(QStringLiteral("^ +| +$"));
        parts.first.replace(whitespace, " ").remove(ends);
        parts.second.replace(whitespace, " ").remove(ends);
    }
    const auto candidate = [&](double candidateSize, bool clamp, bool collect = false) {
        TitleLayout value;
        value.size = candidateSize;
        auto first = shape(parts.first, main, candidateSize, width, transport ? 0 : 1.4,
                           transport ? 0 : -.6, false, transport, clamp, collect, budget);
        auto second = shape(parts.second, subtitle, candidateSize * .45, width, 1.45,
                            candidateSize * .45 * .01, true, false, clamp, collect, budget);
        const double gap = parts.second.isEmpty() ? 0 : candidateSize * .45 * .48;
        for (auto &glyph : second.glyphs)
            glyph.y += first.height + gap;
        value.glyphs = first.glyphs + second.glyphs;
        value.mainLines = first.lines;
        value.height = first.height + gap + second.height;
        value.naturalWidth = first.naturalWidth;
        value.overflow = first.lines > 2 || value.height > budget + 1;
        return value;
    };
    if (transport) {
        result = candidate(size, false, true);
        result.fitted = text;
        if (result.naturalWidth > width) {
            auto dots = shape(QStringLiteral("…"), main, size, width, 0, 0, false, true, false);
            QString fitted;
            for (const auto &glyph : result.glyphs) {
                if (glyph.x + glyph.width + dots.naturalWidth > width)
                    break;
                fitted += glyph.text;
            }
            result.fitted = dots.naturalWidth <= width ? fitted + QStringLiteral("…") : QString();
            result.glyphs =
                    shape(result.fitted, main, size, width, 0, 0, false, true, false).glyphs;
        }
        result.height = 29;
    } else {
        result = candidate(size, false);
        if (result.mainLines > 2 || result.height > budget + 1) {
            const double minimum = std::min(size, narrow ? 18.0 : 20.0);
            int fit = -1;
            for (int i = 1; i <= 8; ++i) {
                result = candidate(size - (size - minimum) * i / 8, false);
                if (result.mainLines <= 2 && result.height <= budget + 1) {
                    fit = i;
                    break;
                }
            }
            if (fit < 0)
                result = candidate(minimum, true);
            else {
                const double high = size - (size - minimum) * (fit - 1) / 8;
                const double low = size - (size - minimum) * fit / 8;
                for (int i = 1; i <= 16; ++i) {
                    result = candidate(high - (high - low) * i / 16, false);
                    if (result.mainLines <= 2 && result.height <= budget + 1)
                        break;
                }
            }
        }
        result = candidate(result.size, result.overflow, true);
        result.height = std::min(result.height, budget);
    }
    qsizetype bytes = sizeof(TitleLayout) + keyBytes.size() * 3 + text.size() * 2;
    for (const auto &glyph : result.glyphs) {
        bytes += sizeof(TitleGlyph) + (glyph.text.size() + glyph.key.size()) * 2;
        for (const auto &run : glyph.renderRuns)
            bytes += sizeof(ResolvedGlyphRun) + run.fontHead.size()
                    + run.indexes.size() * (sizeof(quint32) + sizeof(QPointF));
    }
    // Entry and byte LRU bounds complement each other; oversized inputs are never cached.
    if (m_keys.size() >= 96)
        m_cache.remove(m_keys.takeFirst());
    if (bytes <= CacheBytes) {
        m_cache.insert(key, new TitleLayout(result), int(bytes));
        m_keys.append(key);
    }
    return result;
}
quint64 TextLayoutAdapter::misses() const
{
    return m_misses;
}
int TextLayoutAdapter::cacheBytes() const
{
    return m_cache.totalCost();
}
int TextLayoutAdapter::cacheEntries() const
{
    return m_cache.size();
}
void TextLayoutAdapter::clear()
{
    m_cache.clear();
    m_keys.clear();
}
}
