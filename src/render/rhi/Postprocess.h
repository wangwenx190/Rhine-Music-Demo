#pragma once
#include "RhiApi.h"
#include <QJsonArray>
#include <QJsonObject>
#include <memory>
#include <vector>

namespace yalahoo::rhi {
class ResourceInventory;
struct PostSettings
{
    QSize size;
    int samples = 0;
    float resolution = 1, nearPlane = 5, farPlane = 300, focus = 25, aperture = .0003f;
    float projectionX = 1, projectionY = 1;
    float radius = .18f;
    bool bokeh = false, smaa = false;
};
class Postprocess
{
public:
    Postprocess();
    ~Postprocess();
    void reset();
    void prepare(QRhi *api, const PostSettings &settings, QRhiTexture *color, QRhiTexture *aoNormal,
                 QRhiTexture *aoDepth, QRhiTexture *dofDepth);
    void render(QRhiCommandBuffer *cb);
    QRhiTexture *output() const;
    // Review returns intermediate targets; ordinary builds return nullptr.
    QRhiTexture *smaaEdges() const;
    QRhiTexture *smaaWeights() const;
    QJsonArray passes() const;
    QJsonObject submittedBokeh() const;
    QRhiTexture *rawAo() const;
    QRhiTexture *blurredAo() const;
    quint64 revision() const;
    qint64 uniformUploadBytes() const;
    void inventory(ResourceInventory &inventory) const;

private:
    struct State;
    std::unique_ptr<State> m_state;
    quint64 m_revision = 0;
};
}
