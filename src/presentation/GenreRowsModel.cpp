#include <QCoreApplication>
#include "GenreRowsModel.h"
#include <QFontMetricsF>
#include <QGuiApplication>
#include <cmath>
#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
// Row packing uses measured ink width plus the source chip padding and inter-chip gap.
constexpr auto ChipHorizontalPaddingPixels = 22;
constexpr auto ChipGapPixels = 7;

constexpr int entriesRole = Qt::UserRole + 1;
}
GenreRowsModel::GenreRowsModel(QObject *parent) : QAbstractListModel(parent)
{
    if (qGuiApp)
        connect(qGuiApp, &QGuiApplication::fontDatabaseChanged, this, &GenreRowsModel::measure);
    measure();
}
QVariantList GenreRowsModel::genres() const
{
    return m_genres;
}
void GenreRowsModel::setGenres(const QVariantList &genres)
{
    if (m_genres == genres)
        return;
    m_genres = genres;
    measure();
    Q_EMIT genresChanged();
}
QFont GenreRowsModel::font() const
{
    return m_font;
}
void GenreRowsModel::setFont(const QFont &font)
{
    if (m_font == font)
        return;
    m_font = font;
    measure();
    Q_EMIT fontChanged();
}
double GenreRowsModel::width() const
{
    return m_width;
}
void GenreRowsModel::setWidth(double width)
{
    if (!std::isfinite(width))
        return;
    width = std::max(0., width);
    if (m_width == width)
        return;
    m_width = width;
    rewrap();
    Q_EMIT widthChanged();
}
// Measure once per content/font revision, including source chip padding. Rewrap reuses these
// widths, so resizing does not repeatedly shape the same label for every candidate row.
void GenreRowsModel::measure()
{
    const QFontMetricsF metrics(m_font);
    m_labels.clear();
    m_labels.reserve(m_genres.size() + 1);
    const auto append = [this, &metrics](const QString &id, const QString &name) {
        m_labels.append(
                { id, name,
                  std::ceil(metrics.horizontalAdvance(name)) + ChipHorizontalPaddingPixels });
    };
    append({ }, QCoreApplication::translate("Yalahoo", "All"));
    for (const auto &value : std::as_const(m_genres)) {
        const auto genre = value.toMap();
        append(genre.value(u"id"_s).toString(), genre.value(u"name"_s).toString());
    }
    rewrap();
}
void GenreRowsModel::rewrap()
{
    QList<QVariantList> rows;
    QVariantList row;
    double used = 0;
    for (const auto &label : std::as_const(m_labels)) {
        const double size = std::min(m_width, label.width);
        if (!row.isEmpty() && used + ChipGapPixels + size > m_width) {
            rows.append(row);
            row.clear();
            used = 0;
        }
        row.append(QVariantMap{
                { u"id"_s, label.id }, { u"name"_s, label.name }, { u"size"_s, size } });
        used += (row.size() > 1 ? ChipGapPixels : 0) + size;
    }
    if (!row.isEmpty())
        rows.append(row);
    if (rows == m_rows)
        return;
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
}
int GenreRowsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}
QVariant GenreRowsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.model() != this || index.column() != 0 || index.row() < 0
        || index.row() >= rowCount() || role != entriesRole)
        return { };
    return m_rows[index.row()];
}
QHash<int, QByteArray> GenreRowsModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = { { entriesRole, "entries"_ba } };
    return roles;
}
}
