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
