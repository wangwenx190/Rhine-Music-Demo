#pragma once
#include "SurfaceTransition.h"
#include <QAbstractListModel>
#include <QSet>
#include <array>
#include <memory>

namespace yalahoo {
struct OverviewColumn final
{
    int lane = -1;
    QString name;
    int count = 0;
    double x = 0, y = 0, extent = 1;
    bool selected = false;
};
class OverviewLabelsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(OverviewLabelsModel)
public:
    static constexpr int capacity = 18; // Nine current and at most nine retiring anchors.
    enum Role {
        Lane = Qt::UserRole + 1,
        Name,
        Count,
        X,
        Y,
        Extent,
        Selected,
        Interactive,
        Expanded,
        Fade,
        Offset,
        EntryWidth,
        EntryOpacity,
        EntryOffset
    };
    explicit OverviewLabelsModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = { }) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    static QVector<OverviewColumn> visibleLabels(QVector<OverviewColumn> columns, double width,
                                                 double height, const QSet<int> &retained = { },
                                                 int expanded = -1);
    void update(const QVector<OverviewColumn> &columns, double width, double height, bool ready,
                double dt, bool reduced);
    bool expand(int lane);
    void collapse();
    bool canEnter(int lane) const;
    int expandedLane() const;
    void clear();
    int liveCount() const;
    bool running() const;

private:
    struct Slot final
    {
        OverviewColumn column;
        SurfaceTransition fade{ 460, 240, false, false };
        bool wanted = false;
        double entryTime = 0, entryFrom = 0, entry = 0;
        double opacityTime = 0, opacityFrom = 0, entryOpacity = 0;
        double entryTarget = 0;
    };
    std::array<std::unique_ptr<Slot>, capacity> m_slots;
    int m_expanded = -1;
};
}
