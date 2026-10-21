// Maps stable album selection onto physical archive instances and cover leases. It coordinates
// navigation and scene invalidation without changing playback intent.

#pragma once
#include "SceneGeometry.h"
#include "ArchivePool.h"
#include "MotionController.h"
#include "SceneFrame.h"
#include "RealColumns.h"
#include "ColumnMemory.h"
#include "DetailMotion.h"
#include "CaseClarity.h"
#include "ReturningCasePool.h"
#include "PointerDragController.h"
#include <QPointer>
#include <limits>

namespace yalahoo {

class OverviewController;
class SelectionController final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SelectionController)
    Q_PROPERTY(QString albumId READ albumId NOTIFY selectionChanged FINAL)
    Q_PROPERTY(bool realistic READ realistic WRITE setRealistic NOTIFY arrayModeChanged FINAL)
    Q_PROPERTY(bool rememberColumnPosition READ rememberColumnPosition WRITE setRememberColumnPosition NOTIFY memoryChanged FINAL)
    Q_PROPERTY(bool canMovePrevious READ canMovePrevious NOTIFY selectionChanged FINAL)
    Q_PROPERTY(bool canMoveNext READ canMoveNext NOTIFY selectionChanged FINAL)
public:
    explicit SelectionController(MotionController *motion, QObject *parent = nullptr);
    void setColumns(QList<LibraryColumn> columns, const Snapshot &snapshot = { });
    QPair<QString, QString> coverSource(const QString &id) const;
    QString albumTitle(const QString &id) const;
    QString albumId() const;
    ArchiveCell cell() const;
    ArchiveCell coordinateOrigin() const;
    const ArchivePool &pool() const;
    const QVector<ArchiveInstance> &instances() const;
    Q_INVOKABLE bool select(const QString &id);
    Q_INVOKABLE bool moveRow(int direction);
    Q_INVOKABLE bool moveLane(int direction);
    Q_INVOKABLE bool enterColumn(int lane, bool guided = true);
    std::optional<ArchiveCell> columnTarget(int lane, bool guided = false) const;
    Q_INVOKABLE bool navigateRow(double row);
    bool realistic() const;
    void setRealistic(bool realistic);
    bool rememberColumnPosition() const;
    void setRememberColumnPosition(bool enabled);
    NavigationAvailability navigationAvailability() const;
    bool canMovePrevious() const;
    bool canMoveNext() const;
    double browseRow(int lane) const;
    void setOverview(OverviewController *overview);
    double overviewProgress() const;
    double centerLane() const;
    double columnExtent(double lane) const;
    void overviewAlbumClicked();
    const RealColumns &realColumns() const;
    bool selectCell(ArchiveCell cell, bool guided = false);
    void setLift(double lift);
    void setDetail(double detail);
    void setRotation(double radians);
    void returnSelected();
    void requestDetail(bool detail);
    bool canSelect(const QString &id) const;
    QString adjacentAlbum(const QString &id, int direction) const;
    bool albumNavigationEnabled(const QString &id, int direction) const;
    bool switchDetail(const QString &id);
    double selectedClarity() const
    {
        return m_clarity.value();
    }
    const DetailMotion &detailMotion() const
    {
        return m_detailMotion;
    }
    void updateDetailReadiness(double dt, bool cameraSettled);
    bool archiveReady() const;
    bool archiveInteractive() const;
    bool canInspect() const;
    bool beginDrag(QPointF point, int pointerId = 0);
    bool moveDrag(QPointF point, int pointerId = 0);
    void endDrag(QPointF point, int pointerId = 0);
    void cancelDrag();
    bool dragging() const
    {
        return m_drag.dragging();
    }
    std::array<double, 2> navigationSpeeds() const;
    double rotation() const
    {
        return m_rotation;
    }
    Kinematics liftState() const
    {
        return m_lift;
    }
    const ReturningCasePool &returningCases() const
    {
        return m_returning;
    }
    void advance(double dt);
    void setOpeningTime(std::optional<double> time);
    ArchiveCell rebase();
    double field(double row, double lane) const;
    QVariantMap hitTest(const SceneFrame &frame, double x, double y) const;
    SceneVector selectedPosition() const;
    QQuaternion selectedRotation() const;
    quint64 revision() const;
Q_SIGNALS:
    void selectionChanged();
    void frameChanged();
    void coordinatesRebased(double lane, double row);
    void libraryChanged();
    void arrayModeChanged();
    void memoryChanged();

private:
    double realisticField(double row, double lane, bool reduced) const;
    void advanceLift(double dt, bool reduced, bool detailNavigation);
    struct Pulse final
    {
        ArchiveCell cell;
        double time = 0;
    };
    void publish();
    bool tracksSettled() const;
    std::optional<ArchiveCell> selectionCell(const QString &id) const;
    using Copy = ReturningCase;
    std::array<double, 2> displayOffset() const;
    QMatrix4x4 transform(ArchiveCell cell, double lift = 0, double rotation = 0,
                         bool folded = true) const;
    struct ExtentCache final
    {
        double lane = std::numeric_limits<double>::infinity();
        double extent = 1;
    };
    mutable std::array<ExtentCache, ArchivePool::lanes> m_extentCache;
    mutable double m_extentCenter = 0, m_extentProgress = -1;
    QPointer<OverviewController> m_overview;
    QPointer<MotionController> m_motion;
    ArchivePool m_pool;
    RealColumns m_realColumns;
    ColumnMemory m_memory;
    bool m_realistic = false;
    struct CoverSource final
    {
        QString path, placeholderTitle, title;
    };
    QHash<QString, CoverSource> m_coverSources;
    ArchiveCell m_cell, m_origin;
    QVector<ArchiveInstance> m_instances;
    struct PoolObject final
    {
        QPair<qint64, qint64> physical;
        quint64 objectId = 0;
    };
    // Each complete 9x48 frontier visits every physical residue exactly once.
    // Store its previous identity directly, including across logical coordinate rebases.
    std::array<PoolObject, ArchivePool::capacity> m_poolObjects;
    ReturningCasePool m_returning;
    DetailMotion m_detailMotion;
    CaseClarity m_clarity;
    PointerDragController m_drag;
    bool m_managedDetail = false, m_navigationLift = false;
    QVector<Pulse> m_pulses;
    ScalarSpring m_lift;
    double m_time = 0, m_scanTime = yalahoo::sceneGeometry::SettledSceneSeconds, m_scanBlend = 0;
    double m_lastInteraction = 0, m_idleGain = 0, m_pulseGain = 1, m_rotation = 0;
    double m_liftTarget = yalahoo::sceneGeometry::PreviewLift, m_detail = 0;
    std::optional<double> m_openingTime;
    std::optional<double> m_returnY;
    quint64 m_revision = 0, m_objectSerial = ArchivePool::capacity;
    quint64 m_selectedObject = ++m_objectSerial;
};
}
