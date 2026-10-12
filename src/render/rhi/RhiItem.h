#pragma once
#include "SceneFrame.h"
#include <QQuickRhiItem>
#include <QImage>
#include <QJsonObject>
#include <functional>
#include <memory>

namespace yalahoo::rhi {
// Values only. No QObject or QRhi ownership crosses the synchronization boundary.
struct Input
{
    SceneFrame scene;
    QVector<QMatrix4x4> entities;
    QImage cover;
    QVector4D rowSettings;
    QVector3D rowColumn, lightColumn, spotPosition, spotTarget, spotColor;
    QVector3D keyColor, skyColor, groundColor, lightIntensities, background;
    float printAmbient = .5f, rowDetail = 0, rowFill = 1, spotIntensity = 0,
          environmentIntensity = .25f;
    int shadowSize = 2048;
    bool shadows = true, geometryDiagnostic = false, shadowDiagnostic = false;
    quint64 request = 0;
};
struct Capture
{
    quint64 request = 0;
    QImage color, normal, depth, shadow;
    QJsonObject metadata;
};
// Review/next-backend integration boundary; product replacement belongs to R10D.
class RhiItem final : public QQuickRhiItem
{
    Q_OBJECT
public:
    explicit RhiItem(QQuickItem *parent = nullptr);
    void submit(Input input, QSize target);
    const Input &input() const
    {
        return m_input;
    }
    // The callable is copied during synchronize; callers use a guarded GUI receiver.
    std::function<void(Capture)> captured;

protected:
    QQuickRhiItemRenderer *createRenderer() override;

private:
    Input m_input;
};
}
