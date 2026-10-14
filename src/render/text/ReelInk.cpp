#include "ReelInk.h"
#include "ReelTextNode.h"
#include <QSGClipNode>
#include <QSGTextNode>
#include <QSGTransformNode>
#include <atomic>

namespace yalahoo {
namespace {
std::atomic<int> nodes{ 0 };
std::atomic<quint64> rebuilds{ 0 };
struct FaceNode
{
    QSGTransformNode *transform = nullptr;
    QSGTextNode *text = nullptr;
    double offset = 0;
};
class InkNode : public QSGNode
{
public:
    InkNode()
    {
        ++nodes;
    }
    ~InkNode() override
    {
        --nodes;
    }
    quint64 revision = std::numeric_limits<quint64>::max();
    QColor color;
    QList<FaceNode> faces;
};
}
ReelInk::ReelInk(QQuickItem *parent) : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}
ReelSlot *ReelInk::slot() const
{
    return m_slot;
}
void ReelInk::setSlot(ReelSlot *slot)
{
    if (m_slot == slot)
        return;
    if (m_slot)
        disconnect(m_slot, nullptr, this, nullptr);
    m_slot = slot;
    if (slot) {
        connect(slot, &ReelSlot::contentChanged, this, &ReelInk::update);
        connect(slot, &ReelSlot::frameChanged, this, &ReelInk::update);
    }
    emit slotChanged();
    update();
}
QColor ReelInk::color() const
{
    return m_color;
}
void ReelInk::setColor(const QColor &color)
{
    if (m_color == color)
        return;
    m_color = color;
    emit colorChanged();
    update();
}
int ReelInk::liveNodes()
{
    return nodes;
}
quint64 ReelInk::geometryUpdates()
{
    return rebuilds;
}
QSGNode *ReelInk::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    auto *node = static_cast<InkNode *>(old);
    if (!m_slot || !m_slot->shown()) {
        delete node;
        return nullptr;
    }
    if (!node)
        node = new InkNode;
    if (node->revision != m_slot->revision) {
        while (auto *child = node->firstChild()) {
            node->removeChildNode(child);
            delete child;
        }
        node->faces.clear();
        for (const auto &face : m_slot->faces) {
            if (face.glyph.text.isEmpty() || face.glyph.colorGlyph)
                continue;
            auto *transform = new QSGTransformNode;
            auto *clip = new QSGClipNode;
            clip->setIsRectangular(true);
            clip->setClipRect({ -face.glyph.size * .15 / face.glyph.scale, 0,
                                (m_slot->capacityWidth + face.glyph.size * .3) / face.glyph.scale,
                                face.glyph.height / face.glyph.scale });
            auto *text = createReelTextNode(window(), face.glyph, m_color);
            transform->appendChildNode(clip);
            clip->appendChildNode(text);
            node->appendChildNode(transform);
            node->faces.append({ transform, text, face.offset });
        }
        node->revision = m_slot->revision;
        node->color = m_color;
        ++rebuilds;
    }
    int i = 0;
    for (const auto &face : m_slot->faces) {
        if (face.glyph.text.isEmpty() || face.glyph.colorGlyph)
            continue;
        auto &target = node->faces[i++];
        QMatrix4x4 matrix;
        matrix.translate(float(m_slot->padding()), float(face.offset - m_slot->rollPosition()));
        matrix.scale(float(face.glyph.scale));
        target.transform->setMatrix(matrix);
        if (node->color != m_color)
            recolorReelTextNode(target.text, m_color);
    }
    node->color = m_color;
    return node;
}
}
