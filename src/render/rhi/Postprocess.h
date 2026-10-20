// Owns the RHI postprocessing resource graph and pass submission. Attachments, uniforms and
// sampling dependencies are rebuilt only when their inputs change.

#pragma once
#include "RhiApi.h"
#include <QJsonArray>
#include <QJsonObject>
#include <memory>
#include <vector>

namespace yalahoo::rhi {
namespace postDefaults {
// Source scene clip/focus distances and postprocessing defaults.
inline constexpr auto NearPlane = 5;
inline constexpr auto FarPlane = 300;
inline constexpr auto FocusDistance = 25;
inline constexpr auto Aperture = .0003f;
inline constexpr auto AoRadius = .18f;
}

class ResourceInventory;
struct PostSettings final
{
    QSize size;
    int samples = 0;
    float nearPlane = postDefaults::NearPlane, farPlane = postDefaults::FarPlane,
          focus = postDefaults::FocusDistance, aperture = postDefaults::Aperture;
    float projectionX = 1, projectionY = 1;
    float radius = postDefaults::AoRadius;
    bool bokeh = false, smaa = false;
};
class Postprocess final
{
    Q_DISABLE_COPY_MOVE(Postprocess)
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
    QJsonObject graph(QRhiTexture *expectedSource) const;
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
