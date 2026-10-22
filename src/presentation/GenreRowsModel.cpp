#include "GenreRowsModel.h"
#include <memory>
#include <QAbstractListModel>
#include <QFont>
#include <QVariantList>
#include <QCoreApplication>
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
struct GenreRowsModel::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    explicit Private(GenreRowsModel *owner) : q(owner)
    {
    }
    GenreRowsModel *const q;

    void measure();
    void rewrap();
    struct Label final
    {
        QString id, name;
        double width = 0;
    };
    QVariantList m_genres;
    QFont m_font;
    double m_width = 0;
    QList<Label> m_labels;
    QList<QVariantList> m_rows;
};
GenreRowsModel::GenreRowsModel(QObject *parent)
    : QAbstractListModel(parent), d(std::make_unique<Private>(this))
{
    if (qGuiApp)
        connect(qGuiApp, &QGuiApplication::fontDatabaseChanged, this, [this] {
            d->measure();
        });
    d->measure();
}
QVariantList GenreRowsModel::genres() const
{
    return d->m_genres;
}
void GenreRowsModel::setGenres(const QVariantList &genres)
{
    if (d->m_genres == genres)
        return;
    d->m_genres = genres;
    d->measure();
    Q_EMIT genresChanged();
}
QFont GenreRowsModel::font() const
{
    return d->m_font;
}
void GenreRowsModel::setFont(const QFont &font)
{
    if (d->m_font == font)
        return;
    d->m_font = font;
    d->measure();
    Q_EMIT fontChanged();
}
double GenreRowsModel::width() const
{
    return d->m_width;
}
void GenreRowsModel::setWidth(double width)
{
    if (!std::isfinite(width))
        return;
    width = std::max(0., width);
    if (d->m_width == width)
        return;
    d->m_width = width;
    d->rewrap();
    Q_EMIT widthChanged();
}
// Measure once per content/font revision, including source chip padding. Rewrap reuses these
// widths, so resizing does not repeatedly shape the same label for every candidate row.
void GenreRowsModel::Private::measure()
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
void GenreRowsModel::Private::rewrap()
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
    q->beginResetModel();
    m_rows = std::move(rows);
    q->endResetModel();
}
int GenreRowsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(d->m_rows.size());
}
QVariant GenreRowsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.model() != this || index.column() != 0 || index.row() < 0
        || index.row() >= rowCount() || role != entriesRole)
        return { };
    return d->m_rows[index.row()];
}
QHash<int, QByteArray> GenreRowsModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = { { entriesRole, "entries"_ba } };
    return roles;
}
GenreRowsModel::~GenreRowsModel() = default;

}
