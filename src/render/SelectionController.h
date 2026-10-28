// Maps stable album selection onto physical archive instances and cover leases. It coordinates
// navigation and scene invalidation without changing playback intent.

#pragma once
#include <memory>
#include "SceneGeometry.h"
#include "ArchivePool.h"
#include "MotionController.h"
#include "SceneFrame.h"
#include "RealColumns.h"
#include "ColumnMemory.h"
#include "DetailMotion.h"
#include "ReturningCasePool.h"

namespace yalahoo {

struct CoverSource final
{
    QString path, placeholderTitle;
    bool operator==(const CoverSource &) const = default;
};
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
    ~SelectionController() override;
    void setColumns(QList<LibraryColumn> columns, const Snapshot &snapshot = { });
    CoverSource coverSource(const QString &id) const;
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
    double selectedClarity() const;
    const DetailMotion &detailMotion() const;
    void updateDetailReadiness(double dt, bool cameraSettled);
    bool archiveReady() const;
    bool archiveInteractive() const;
    bool canInspect() const;
    bool beginDrag(QPointF point, int pointerId = 0);
    bool moveDrag(QPointF point, int pointerId = 0);
    void endDrag(QPointF point, int pointerId = 0);
    void cancelDrag();
    bool dragging() const;
    std::array<double, 2> navigationSpeeds() const;
    double rotation() const;
    Kinematics liftState() const;
    const ReturningCasePool &returningCases() const;
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
    struct Private;
    std::unique_ptr<Private> d;
};
}
