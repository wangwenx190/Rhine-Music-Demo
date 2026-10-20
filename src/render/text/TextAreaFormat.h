#pragma once
#include <QObject>
#include <QPointer>
#include <QQuickTextDocument>

namespace yalahoo {
// CSS line boxes for the plain-text editor; Qt keeps selection, IME and undo ownership.
class TextAreaFormat : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TextAreaFormat)
public:
    TextAreaFormat() = default;

private:
    Q_PROPERTY(QQuickTextDocument *document READ document WRITE setDocument NOTIFY changed FINAL)
    Q_PROPERTY(qreal lineHeight READ lineHeight WRITE setLineHeight NOTIFY changed FINAL)
public:
    using QObject::QObject;
    QQuickTextDocument *document() const;
    void setDocument(QQuickTextDocument *document);
    qreal lineHeight() const;
    void setLineHeight(qreal height);
Q_SIGNALS:
    void changed();

private:
    void apply(int position, int added, bool all);
    QPointer<QQuickTextDocument> m_document;
    qreal m_lineHeight = 20.4;
    bool m_applying = false;
    QMetaObject::Connection m_connection;
};
}
