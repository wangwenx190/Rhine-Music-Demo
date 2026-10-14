#include "ReelTextNode.h"
#include "TextLayoutAdapter.h"
#include <QQuickWindow>
#include <QSGTextNode>
#include <QRawFont>
#include <QtQuick/private/qsginternaltextnode_p.h>
#include <QtQuick/private/qsgadaptationlayer_p.h>

namespace yalahoo {
QSGTextNode *createReelTextNode(QQuickWindow *window, const TitleGlyph &glyph, QColor color)
{
    auto *node = window->createTextNode();
    node->setRenderType(QSGTextNode::CurveRendering);
    node->setColor(color);
    auto *internal = static_cast<QSGInternalTextNode *>(node);
    for (const auto &data : glyph.renderRuns) {
        const auto font = QRawFont::fromFont(data.font);
        if (!font.isValid() || font.fontTable("head") != data.fontHead) {
            qWarning("Render-thread font identity differs from measured glyph font");
            return node;
        }
        QGlyphRun run;
        run.setRawFont(font);
        run.setGlyphIndexes(data.indexes);
        run.setPositions(data.positions);
        run.setFlags(data.flags);
        internal->addGlyphs({ }, run, color, nullptr);
    }
    return node;
}
void recolorReelTextNode(QSGTextNode *node, QColor color)
{
    node->setColor(color);
    const auto visit = [&](const auto &self, QSGNode *root) -> void {
        if (auto *glyph = dynamic_cast<QSGGlyphNode *>(root)) {
            glyph->setColor(color);
            glyph->update();
        }
        for (auto *child = root->firstChild(); child; child = child->nextSibling())
            self(self, child);
    };
    visit(visit, node);
}
}
