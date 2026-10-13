#pragma once
#include "ArchivePool.h"
#include "MotionController.h"
#include "SceneFrame.h"
#include <QPointer>

namespace yalahoo {
class SelectionController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString albumId READ albumId NOTIFY selectionChanged)
public:
    explicit SelectionController(MotionController *motion, QObject *parent = nullptr);
    void setColumns(QList<LibraryColumn> columns, Snapshot snapshot = { });
    QPair<QString, QString> coverSource(const QString &id) const;
    QString albumId() const;
    ArchiveCell cell() const;
    ArchiveCell coordinateOrigin() const;
    const ArchivePool &pool() const;
    const QVector<ArchiveInstance> &instances() const;
    Q_INVOKABLE bool select(const QString &id);
    Q_INVOKABLE bool moveRow(int direction);
    Q_INVOKABLE bool moveLane(int direction);
    bool selectCell(ArchiveCell cell, bool guided = false);
    void setLift(double lift);
    void setDetail(double detail);
    void setRotation(double radians);
    void returnSelected();
    void advance(double dt);
    ArchiveCell rebase();
    double field(double row, double lane) const;
    QVariantMap hitTest(const SceneFrame &frame, double x, double y) const;
    SceneVector selectedPosition() const;
    QQuaternion selectedRotation() const;
    quint64 revision() const;
signals:
    void selectionChanged();
    void frameChanged();
    void coordinatesRebased(double lane, double row);
    void libraryChanged();

private:
    struct Copy
    {
        ArchiveCell cell;
        QString albumId;
        ScalarSpring lift;
        double rotation = 0;
        std::optional<double> returnY;
        quint64 objectId = 0;
    };
    struct Pulse
    {
        ArchiveCell cell;
        double time = 0;
    };
    void publish();
    std::array<double, 2> displayOffset() const;
    QMatrix4x4 transform(ArchiveCell cell, double lift = 0, double rotation = 0) const;
    QPointer<MotionController> m_motion;
    ArchivePool m_pool;
    QHash<QString, QPair<QString, QString>> m_coverSources;
    ArchiveCell m_cell, m_origin;
    QVector<ArchiveInstance> m_instances;
    QHash<QPair<qint64, qint64>, quint64> m_poolObjects;
    QVector<Copy> m_returning;
    QVector<Pulse> m_pulses;
    ScalarSpring m_lift;
    double m_time = 0, m_scanTime = 26.56, m_scanBlend = 0;
    double m_lastInteraction = 0, m_idleGain = 0, m_pulseGain = 1, m_rotation = 0;
    double m_liftTarget = .9, m_detail = 0;
    std::optional<double> m_returnY;
    quint64 m_revision = 0, m_objectSerial = ArchivePool::capacity;
    quint64 m_selectedObject = ++m_objectSerial;
};
}
