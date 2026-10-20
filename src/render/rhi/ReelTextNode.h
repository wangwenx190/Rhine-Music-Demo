// Builds scene-graph text nodes from already shaped glyph runs. Nodes own rendering resources
// while GUI text layout remains outside the render callback.

#pragma once
#include <QColor>
class QSGTextNode;
class QQuickWindow;
namespace yalahoo {
struct TitleGlyph;
// Qt Quick private glyph submission stays behind this render boundary. Value shaping is public Qt.
QSGTextNode *createReelTextNode(QQuickWindow *window, const TitleGlyph &glyph, QColor color);
void recolorReelTextNode(QSGTextNode *node, QColor color);
}
