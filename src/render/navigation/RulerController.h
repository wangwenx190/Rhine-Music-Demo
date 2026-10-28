// Builds bounded ruler ticks, labels and navigation state in C++. Source-derived geometry is
// published to the painter rather than recomputed in QML.

#pragma once
#include <QPointer>
#include "ScalarSpring.h"
#include "SelectionController.h"
#include <QVariantList>
#include <array>

namespace yalahoo {
namespace rulerGeometry {
// Logical source tick geometry and a bounded overscanned physical pool.
inline constexpr auto PhysicalTickCount = 16;
inline constexpr auto TickPixels = 3;
inline constexpr auto GapPixels = 8;
inline constexpr auto ActiveHeightPixels = 30;
inline constexpr auto InactiveRippleTime = 1e20;
}

class InputRouter;
struct RulerMark final
{
    double x = 0, height = 0, alpha = 0;
    bool visible = false, active = false, hovering = false;
    bool operator==(const RulerMark &) const = default;
};
struct RulerDrawing final
{
    std::array<RulerMark, rulerGeometry::PhysicalTickCount> marks;
    double width = 0, tick = rulerGeometry::TickPixels, gap = rulerGeometry::GapPixels,
           height = rulerGeometry::ActiveHeightPixels;
    bool overflow = false;
    bool operator==(const RulerDrawing &) const = default;
};
class RulerController final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(RulerController)
    Q_PROPERTY(QVariantList slotStates READ slotStates NOTIFY changed FINAL)
    Q_PROPERTY(double width READ width NOTIFY changed FINAL)
    Q_PROPERTY(bool narrow READ narrow WRITE setNarrow NOTIFY geometryChanged FINAL)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged FINAL)
public:
    RulerController(MotionController *motion, SelectionController *selection,
                    QObject *parent = nullptr);
    QVariantList slotStates() const;
    double width() const;
    bool narrow() const;
    void setNarrow(bool narrow);
    bool active() const;
    void setActive(bool active);
    Q_INVOKABLE bool click(int slot);
    Q_INVOKABLE void hover(int slot, bool pointer, bool focused);
    void configure(int count, int row, double physicalRow, bool loop, bool groupChanged);
    void advance(double motionDt);
    void setInputRouter(InputRouter *router);
    const RulerDrawing &drawing() const;
    QVariantMap state() const;
Q_SIGNALS:
    void changed();
    void geometryChanged();
    void activeChanged();
    void inputReceived();

private:
    struct Slot final
    {
        double ordinal = 0, x = 0;
        ScalarSpring offset, presence, height, opacity;
        double revealAt = 0, rippleAt = -rulerGeometry::InactiveRippleTime;
        bool pointer = false, focused = false;
    };
    struct PublishedSlot final
    {
        RulerMark mark;
        double ordinal = 0;
        QString title;
        bool enabled = false;
    };
    void synchronize(bool groupChanged = false);
    void regroup(bool instant);
    void recycle();
    void render(double dt, bool instant);
    void publish();
    bool valid(double ordinal) const;
    double selectionScroll() const;
    QString title(double ordinal) const;
    QPointer<MotionController> m_motion;
    QPointer<SelectionController> m_selection;
    QPointer<InputRouter> m_router;
    std::array<Slot, rulerGeometry::PhysicalTickCount> m_slots;
    RulerDrawing m_drawing;
    QVariantList m_slotStates;
    std::array<PublishedSlot, rulerGeometry::PhysicalTickCount> m_publishedSlots;
    bool m_titlesDirty = true;
    ScalarSpring m_scroll, m_width;
    int m_count = 0, m_lane = -1;
    double m_selected = 0, m_target = 0, m_time = 0;
    double m_lastPhysical = 0;
    bool m_loop = true, m_populated = false, m_narrow = false, m_active = true, m_moving = false;
};
}
