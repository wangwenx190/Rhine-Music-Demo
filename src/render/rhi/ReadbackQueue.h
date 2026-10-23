// Owns bounded asynchronous GPU captures, scalar-alignment textures and completion state.
// The renderer drains the queue before changing devices or destroying QRhi resources.

#pragma once
#include "RhiApi.h"
#include "RhiItem.h"
#include <array>
#include <vector>

namespace yalahoo::rhi {
class ResourceInventory;
// The ordinal order matches the capture texture/output tables; aliases share a readback owner.
enum CaptureField {
    OpaqueColorField,
    OpaqueNormalField,
    OpaqueDepthField,
    ShadowField,
    CompositeColorField,
    CompositeNormalField,
    CompositeDepthField,
    PostprocessedField,
    AtlasField,
    RawAoField,
    BlurredAoField,
    AoNormalField,
    AoDepthField,
    DofDepthField,
    CaptureFieldCount
};
class ReadbackQueue final
{
    Q_DISABLE_COPY_MOVE(ReadbackQueue)
public:
    ReadbackQueue() = default;
    ~ReadbackQueue();
    bool hasCapacity() const;
    void retire();
    bool drain();
    void finish(QRhi *api);
    void reset();
    void enqueue(QRhi *api, QRhiCommandBuffer *cb,
                 const std::array<QRhiTexture *, CaptureFieldCount> &textures, Capture capture,
                 const Input &input, std::function<void(Capture)> deliver,
                 std::function<void(QString)> failed,
                 const std::function<QJsonObject()> &inventory);
    void inventory(ResourceInventory &inventory) const;
    quint64 revision() const
    {
        return alignedReadbackRevision;
    }

private:
    friend class ReadbackQueueTest;
    struct Pending;
    static void completeReadback(const std::weak_ptr<Pending> &weak, const int &field);
    QRhiTexture *prepareReadback(QRhi *api, QRhiTexture *texture, int field,
                                 QRhiResourceUpdateBatch *reads, Pending &pending,
                                 QJsonArray &alignedCopies);
    std::array<std::unique_ptr<QRhiTexture>, CaptureFieldCount> alignedReadbacks;
    int readbackPackAlignment = 0;
    quint64 alignedReadbackRevision = 0;
    std::vector<std::shared_ptr<Pending>> pending;
};
}
