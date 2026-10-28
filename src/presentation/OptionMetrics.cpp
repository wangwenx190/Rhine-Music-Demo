#include "OptionMetrics.h"
#include <QFontMetricsF>
#include <QGuiApplication>
#include <algorithm>
#include <utility>

namespace yalahoo {
OptionMetrics::OptionMetrics(QObject *parent) : QObject(parent)
{
    if (auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
        connect(application, &QGuiApplication::fontDatabaseChanged, this, &OptionMetrics::measure);
}
QVariantList OptionMetrics::model() const
{
    return m_model;
}
void OptionMetrics::setModel(const QVariantList &model)
{
    if (model == m_model)
        return;
    m_model = model;
    measure();
    Q_EMIT modelChanged();
}
QString OptionMetrics::textRole() const
{
    return m_textRole;
}
void OptionMetrics::setTextRole(const QString &role)
{
    if (role == m_textRole)
        return;
    m_textRole = role;
    measure();
    Q_EMIT textRoleChanged();
}
QFont OptionMetrics::font() const
{
    return m_font;
}
void OptionMetrics::setFont(const QFont &font)
{
    if (font == m_font)
        return;
    m_font = font;
    measure();
    Q_EMIT fontChanged();
}
double OptionMetrics::maximumWidth() const
{
    return m_maximumWidth;
}
void OptionMetrics::measure()
{
    const QFontMetricsF metrics(m_font);
    double width = 0;
    for (const auto &row : std::as_const(m_model)) {
        const auto label = row.metaType() == QMetaType::fromType<QVariantMap>()
                ? row.toMap().value(m_textRole).toString()
                : row.toString();
        width = std::max(width, metrics.horizontalAdvance(label));
    }
    if (width == m_maximumWidth)
        return;
    m_maximumWidth = width;
    Q_EMIT maximumWidthChanged();
}
}
