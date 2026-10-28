#include "TextAreaFormat.h"
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <cmath>

namespace yalahoo {
QQuickTextDocument *TextAreaFormat::document() const
{
    return m_document;
}
void TextAreaFormat::setDocument(QQuickTextDocument *document)
{
    if (m_document == document)
        return;
    disconnect(m_connection);
    m_document = document;
    if (document && document->textDocument()) {
        m_connection = connect(document->textDocument(), &QTextDocument::contentsChange, this,
                               [this](int position, int, int added) {
                                   apply(position, added, false);
                               });
        apply(0, 0, true);
    }
    Q_EMIT changed();
}
qreal TextAreaFormat::lineHeight() const
{
    return m_lineHeight;
}
void TextAreaFormat::setLineHeight(qreal height)
{
    if (!std::isfinite(height) || height <= 0 || m_lineHeight == height)
        return;
    m_lineHeight = height;
    apply(0, 0, true);
    Q_EMIT changed();
}
void TextAreaFormat::apply(int position, int added, bool all)
{
    if (m_applying || !m_document || !m_document->textDocument())
        return;
    auto *document = m_document->textDocument();
    const bool emptyUndo = !document->isUndoAvailable() && !document->isRedoAvailable();
    m_applying = true;
    auto block = all ? document->begin() : document->findBlock(position);
    while (block.isValid() && (all || block.position() <= position + added)) {
        auto format = block.blockFormat();
        if (format.lineHeightType() != QTextBlockFormat::FixedHeight
            || format.lineHeight() != m_lineHeight) {
            QTextCursor cursor(block);
            cursor.joinPreviousEditBlock();
            format.setLineHeight(m_lineHeight, QTextBlockFormat::FixedHeight);
            format.setTopMargin(0);
            format.setBottomMargin(0);
            cursor.setBlockFormat(format);
            cursor.endEditBlock();
        }
        block = block.next();
    }
    if (emptyUndo)
        document->clearUndoRedoStacks();
    m_applying = false;
}
}
