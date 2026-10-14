#pragma once
#include "LayoutMetrics.h"
#include "RulerController.h"
#include "RulerInk.h"
#include "InputRouter.h"
#include "OrdinalMotion.h"
#include <QCanvasPainterItem>
#include <QtQml/qqmlregistration.h>
struct UiLayoutMetrics
{
    Q_GADGET
    QML_FOREIGN(yalahoo::LayoutMetrics)
    QML_NAMED_ELEMENT(LayoutMetrics)
    QML_UNCREATABLE("Injected by review")
};

struct RulerControllerQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::RulerController)
    QML_NAMED_ELEMENT(RulerController)
    QML_UNCREATABLE("Owned by application")
};
struct InputRouterQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::InputRouter)
    QML_NAMED_ELEMENT(InputRouter)
    QML_UNCREATABLE("Owned by application")
};
struct RulerInkQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::RulerInk)
    QML_NAMED_ELEMENT(RulerInk)
};
struct OrdinalMotionQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::OrdinalMotion)
    QML_NAMED_ELEMENT(OrdinalMotion)
};
struct CanvasPainterBase
{
    Q_GADGET
    QML_FOREIGN(QCanvasPainterItem)
    QML_ANONYMOUS
};

struct UiFrameClock
{
    Q_GADGET
    QML_FOREIGN(yalahoo::FrameClock)
    QML_NAMED_ELEMENT(FrameClock)
    QML_UNCREATABLE("Injected by review")
};
struct UiSelectionController
{
    Q_GADGET
    QML_FOREIGN(yalahoo::SelectionController)
    QML_NAMED_ELEMENT(SelectionController)
    QML_UNCREATABLE("Injected by review")
};
