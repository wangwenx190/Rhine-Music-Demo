// Shapes whole text runs and groups glyphs without losing grapheme context. Cached layout
// retains fallback-font identity, baselines and color-glyph descriptors.

#pragma once
#include <QFont>
#include <QGlyphRun>
#include <QCache>

namespace yalahoo {
struct ResolvedGlyphRun final
{
    QFont font;
    QByteArray fontHead;
    QList<quint32> indexes;
    QList<QPointF> positions;
    QGlyphRun::GlyphRunFlags flags;
    bool operator==(const ResolvedGlyphRun &) const = default;
};
struct TitleGlyph final
{
    QString key, text;
    bool translation = false;
    double x = 0, y = 0, width = 0, height = 0, size = 0, scale = 1;
    QFont font;
    QList<ResolvedGlyphRun> renderRuns;
    double baseline = 0;
    bool colorGlyph = false;
};
struct TitleLayout final
{
    QList<TitleGlyph> glyphs;
    QString fitted;
    double size = 0, height = 0, naturalWidth = 0;
    int mainLines = 0;
    bool overflow = false;
};
inline bool sameGlyphInk(const TitleGlyph &a, const TitleGlyph &b)
{
    return a.text == b.text && a.font == b.font && a.height == b.height && a.scale == b.scale
            && a.colorGlyph == b.colorGlyph && a.renderRuns == b.renderRuns;
}
class TextLayoutAdapter final
{
    Q_DISABLE_COPY_MOVE(TextLayoutAdapter)
public:
    static constexpr int CacheBytes = 8 * 1024 * 1024;
    TextLayoutAdapter();
    TitleLayout measure(const QString &text, const QFont &main, const QFont &subtitle, double size,
                        double width, double budget, bool narrow, double dpr,
                        bool transport = false);
    static QList<int> boundaries(const QString &text);
    static QPair<QString, QString> titleParts(const QString &text);
    quint64 misses() const;
    int cacheBytes() const;
    int cacheEntries() const;
    void clear();

private:
    QCache<QString, TitleLayout> m_cache;
    QStringList m_keys;
    quint64 m_misses = 0;
};
}
