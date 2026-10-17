#include "ReelInk.h"
#include "ReelTextNode.h"
#include <QSGClipNode>
#include <QSGTextNode>
#include <QSGTransformNode>
#include <atomic>
#include <algorithm>

namespace yalahoo {
namespace {
std::atomic<int> nodes{ 0 };
std::atomic<quint64> rebuilds{ 0 };
std::atomic<quint64> reused{ 0 };
struct FaceNode
{
    QSGTransformNode *transform = nullptr;
    QSGClipNode *clip = nullptr;
    QSGTextNode *text = nullptr;
    TitleGlyph glyph;
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
    Q_EMIT slotChanged();
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
    Q_EMIT colorChanged();
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
quint64 ReelInk::geometryReuses()
{
    return reused;
}
QSGNode *ReelInk::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    auto *node = static_cast<InkNode *>(old);
    if (!isVisible() || !m_slot || !m_slot->shown()) {
        delete node;
        return nullptr;
    }
    if (!node)
        node = new InkNode;
    if (node->revision != m_slot->revision) {
        auto previous = std::move(node->faces);
        for (const auto &face : previous)
            node->removeChildNode(face.transform);
        node->faces.clear();
        for (const auto &face : m_slot->faces) {
            if (face.glyph.text.isEmpty() || face.glyph.colorGlyph)
                continue;
            const auto found =
                    std::find_if(previous.begin(), previous.end(), [&](const auto &oldFace) {
                        return sameGlyphInk(oldFace.glyph, face.glyph);
                    });
            FaceNode target;
            if (found != previous.end()) {
                ++reused;
                target = *found;
                previous.erase(found);
                if (node->color != m_color)
                    recolorReelTextNode(target.text, m_color);
            } else {
                target.transform = new QSGTransformNode;
                target.clip = new QSGClipNode;
                target.clip->setIsRectangular(true);
                target.text = createReelTextNode(window(), face.glyph, m_color);
                target.glyph = face.glyph;
                target.transform->appendChildNode(target.clip);
                target.clip->appendChildNode(target.text);
                ++rebuilds;
            }
            target.clip->setClipRect(
                    { -face.glyph.size * .15 / face.glyph.scale, 0,
                      (m_slot->capacityWidth + face.glyph.size * .3) / face.glyph.scale,
                      face.glyph.height / face.glyph.scale });
            node->appendChildNode(target.transform);
            node->faces.append(target);
        }
        for (const auto &face : previous)
            delete face.transform;
        node->revision = m_slot->revision;
        node->color = m_color;
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
