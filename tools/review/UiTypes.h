#pragma once
#include "OverviewController.h"
#include "PresentationController.h"
#include "DetailModel.h"
#include "LayoutMetrics.h"
#include "RulerController.h"
#include "RulerInk.h"
#include "InputRouter.h"
#include "OrdinalMotion.h"
#include "TitleReel.h"
#include "ReelInk.h"
#include "TransportMotion.h"
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
struct TitleReelQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::TitleReel)
    QML_NAMED_ELEMENT(TitleReelMotion)
};
struct ReelSlotQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::ReelSlot)
    QML_NAMED_ELEMENT(ReelSlot)
    QML_UNCREATABLE("Owned by TitleReelMotion")
};
struct ReelInkQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::ReelInk)
    QML_NAMED_ELEMENT(ReelInk)
};
struct TransportMotionQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::TransportMotion)
    QML_NAMED_ELEMENT(TransportMotion)
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

struct PresentationControllerQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::PresentationController)
    QML_NAMED_ELEMENT(PresentationController)
    QML_UNCREATABLE("Owned by application")
};
struct SurfaceTransitionQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::SurfaceTransition)
    QML_NAMED_ELEMENT(SurfaceTransition)
    QML_UNCREATABLE("Owned by PresentationController")
};
struct DetailModelQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::DetailModel)
    QML_NAMED_ELEMENT(DetailModel)
    QML_UNCREATABLE("Owned by application")
};

struct OverviewControllerQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::OverviewController)
    QML_NAMED_ELEMENT(OverviewController)
    QML_UNCREATABLE("Owned by application")
};
struct OverviewLabelsModelQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::OverviewLabelsModel)
    QML_NAMED_ELEMENT(OverviewLabelsModel)
    QML_UNCREATABLE("Owned by OverviewController")
};
