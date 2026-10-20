#pragma once
#include <QFont>
#include <QObject>
#include <QVariantList>

namespace yalahoo {
// Computes natural option width when data or font changes, outside QML bindings.
class OptionMetrics : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(OptionMetrics)
    Q_PROPERTY(QVariantList model READ model WRITE setModel NOTIFY modelChanged FINAL)
    Q_PROPERTY(QString textRole READ textRole WRITE setTextRole NOTIFY textRoleChanged FINAL)
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged FINAL)
    Q_PROPERTY(double maximumWidth READ maximumWidth NOTIFY maximumWidthChanged FINAL)
public:
    explicit OptionMetrics(QObject *parent = nullptr);
    QVariantList model() const;
    void setModel(const QVariantList &model);
    QString textRole() const;
    void setTextRole(const QString &role);
    QFont font() const;
    void setFont(const QFont &font);
    double maximumWidth() const;
Q_SIGNALS:
    void modelChanged();
    void textRoleChanged();
    void fontChanged();
    void maximumWidthChanged();

private:
    void measure();
    QVariantList m_model;
    QString m_textRole;
    QFont m_font;
    double m_maximumWidth = 0;
};
}
