#pragma once
#include "LibrarySession.h"
#include <QAbstractListModel>

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
    void prepare(const LibrarySession &library);
    static QString time(double seconds);
signals:
    void albumChanged();

private:
    Snapshot m_snapshot;
    const Album *m_album = nullptr;
    QVariantMap m_info;
};
}
