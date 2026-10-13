#pragma once
#include "RhiItem.h"
#include "SceneAdapter.h"
#include "AtlasAllocator.h"
#include <QPointer>

namespace yalahoo::rhi {
class SceneItem : public RhiItem
{
    Q_OBJECT
    Q_PROPERTY(yalahoo::SceneAdapter* adapter READ adapter WRITE setAdapter NOTIFY adapterChanged)
    Q_PROPERTY(bool geometryOnly READ geometryOnly WRITE setGeometryOnly NOTIFY geometryOnlyChanged)
    Q_PROPERTY(bool telemetryEnabled READ telemetryEnabled WRITE setTelemetryEnabled NOTIFY telemetryEnabledChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
public:
    explicit SceneItem(QQuickItem *parent = nullptr);
    SceneAdapter *adapter() const
    {
        return m_adapter;
    }
    void setAdapter(SceneAdapter *adapter);
    bool geometryOnly() const
    {
        return m_geometryOnly;
    }
    void setGeometryOnly(bool enabled);
    bool telemetryEnabled() const
    {
        return m_telemetryEnabled;
    }
    void setTelemetryEnabled(bool enabled);
    QString errorText() const
    {
        return m_errorText;
    }
    Q_INVOKABLE QVector3D qtProjection(QVector3D world) const;
    QJsonObject coverStatistics() const;
signals:
    void adapterChanged();
    void geometryOnlyChanged();
    void telemetryEnabledChanged();
    void frameSubmitted(QJsonObject sample);
    void errorTextChanged();

protected:
    void updatePolish() override;
    bool event(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void touchEvent(QTouchEvent *event) override;

private:
    void schedule();
    void preparePool(Input &input);
    void resetPool();
    struct OwnedCover
    {
        quint64 objectId = 0, lease = 0;
        std::shared_ptr<SelectedCoverLease> pixels;
        CoverSnapshot tiledPixels;
        QVector<AtlasUpload> uploads;
    };
    AtlasAllocator m_atlas{ 8192, 436, 22 };
    QHash<QString, AtlasLease> m_poolLeases;
    QSet<QString> m_readyCovers;
    std::array<OwnedCover, 4> m_ownedCovers;
    quint64 m_ownedSerial = 0;
    quint64 m_activeObject = 0;
    QPointer<SelectionController> m_poolSelection;
    QMetaObject::Connection m_poolResetConnection;
    bool m_poolMode = false;
    quint64 m_coverRevision = 0, m_publishedCoverRevision = 0;
    QPointer<SceneAdapter> m_adapter;
    QImage m_cover;
    CoverCache m_covers;
    SelectedCoverPool m_coverPool;
    std::shared_ptr<SelectedCoverLease> m_selectedCover;
    QString m_errorText;
    bool m_geometryOnly = false;
    bool m_telemetryEnabled = false;
    std::optional<QPointF> m_touchStart;
    quint64 m_touchTimestamp = 0;
};
}
