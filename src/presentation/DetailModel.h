#pragma once
#include "LibrarySession.h"
#include <QAbstractListModel>
#include <QCache>
#include <QFont>

namespace yalahoo {
class DetailModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap album READ album NOTIFY albumChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY albumChanged)
public:
    enum Role {
        TrackId = Qt::UserRole + 1,
        Number,
        Title,
        Artist,
        Format,
        Duration,
        Disc,
        DiscHeading
    };
    explicit DetailModel(QObject *parent = nullptr) : QAbstractListModel(parent)
    {
    }
    int rowCount(const QModelIndex &parent = { }) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QVariantMap album() const
    {
        return m_info;
    }
    void prepare(const LibrarySession &library, bool metadataOnly = false);
    Q_INVOKABLE double normalLineHeight(const QString &text, const QFont &font);
    static QString time(double seconds);
Q_SIGNALS:
    void albumChanged();

private:
    QCache<QString, double> m_lineHeights{ 256 * 1024 };
    Snapshot m_snapshot;
    const Album *m_album = nullptr;
    Album m_resolved;
    QVariantMap m_info;
};
}
