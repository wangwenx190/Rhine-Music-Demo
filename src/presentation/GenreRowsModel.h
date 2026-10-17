#pragma once
#include <QAbstractListModel>
#include <QFont>
#include <QVariantList>

namespace yalahoo {
// Width changes rewrap measured labels; only visible rows cross the QML boundary.
class GenreRowsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QVariantList genres READ genres WRITE setGenres NOTIFY genresChanged)
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
    Q_PROPERTY(double width READ width WRITE setWidth NOTIFY widthChanged)
public:
    explicit GenreRowsModel(QObject *parent = nullptr);
    QVariantList genres() const;
    void setGenres(const QVariantList &genres);
    QFont font() const;
    void setFont(const QFont &font);
    double width() const;
    void setWidth(double width);
    int rowCount(const QModelIndex &parent = { }) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
Q_SIGNALS:
    void genresChanged();
    void fontChanged();
    void widthChanged();

private:
    void measure();
    void rewrap();
    struct Label
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
}
