// Projects the selected album into detail-page data and metadata status. Stable IDs and snapshot
// ownership keep asynchronous updates separate from navigation.

#pragma once
#include "Units.h"
#include <QObject>
#include "MusicTypes.h"
#include <QAbstractListModel>
#include <QCache>
#include <QFont>

namespace yalahoo {
namespace detailText {
// The line-height cache is bounded by estimated owned bytes.
inline constexpr auto HeightCacheKiB = 256;
}

class LibrarySession;

class DetailModel : public QAbstractListModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DetailModel)
    Q_PROPERTY(QVariantMap album READ album NOTIFY albumChanged FINAL)
    Q_PROPERTY(int count READ rowCount NOTIFY albumChanged FINAL)
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
    QCache<QString, double> m_lineHeights{ detailText::HeightCacheKiB
                                           * yalahoo::units::BytesPerKiB };
    Snapshot m_snapshot;
    const Album *m_album = nullptr;
    Album m_resolved;
    QVariantMap m_info;
};
}
