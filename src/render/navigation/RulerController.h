#pragma once
#include "ScalarSpring.h"
#include "SelectionController.h"
#include <QVariantList>
#include <array>

namespace yalahoo {
class InputRouter;
struct RulerMark
{
    double x = 0, height = 0, alpha = 0;
    bool visible = false, active = false, hovering = false;
    bool operator==(const RulerMark &) const = default;
};
struct RulerDrawing
{
    std::array<RulerMark, 16> marks;
    double width = 0, tick = 3, gap = 8, height = 30;
    bool overflow = false;
    bool operator==(const RulerDrawing &) const = default;
};
class RulerController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList slotStates READ slotStates NOTIFY changed)
    Q_PROPERTY(double width READ width NOTIFY changed)
    Q_PROPERTY(bool narrow READ narrow WRITE setNarrow NOTIFY geometryChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
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
    struct Slot
    {
        double ordinal = 0, x = 0;
        ScalarSpring offset, presence, height, opacity;
        double revealAt = 0, rippleAt = -1e20;
        bool pointer = false, focused = false;
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
    std::array<Slot, 16> m_slots;
    RulerDrawing m_drawing;
    QVariantList m_slotStates;
    ScalarSpring m_scroll, m_width;
    int m_count = 0, m_lane = -1;
    double m_selected = 0, m_target = 0, m_time = 0;
    double m_lastPhysical = 0;
    bool m_loop = true, m_populated = false, m_narrow = false, m_active = true, m_moving = false;
};
}
