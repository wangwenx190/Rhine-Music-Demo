#include "OpeningController.h"
#include "OpeningInk.h"
#pragma once
#include "LibrarySession.h"
#include "LibraryStore.h"
#include "TrackFocusController.h"
#include "PanelController.h"
#include "PresentationController.h"
#include "DetailModel.h"
#include "MotionController.h"
#include "SceneAdapter.h"
#include "OverviewController.h"
#include "SceneItem.h"
#include "RulerController.h"
#include "RulerInk.h"
#include "InputRouter.h"
#include "OrdinalMotion.h"
#include "TitleReel.h"
#include "ReelInk.h"
#include "TransportMotion.h"
#include <QCanvasPainterItem>
#include <QtQml/qqmlregistration.h>

struct LayoutMetricsQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::LayoutMetrics)
    QML_NAMED_ELEMENT(LayoutMetrics)
    QML_UNCREATABLE("Owned by SceneAdapter")
};
struct SceneAdapterQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::SceneAdapter)
    QML_NAMED_ELEMENT(SceneAdapter)
    QML_UNCREATABLE("Injected by the native application")
};
struct SelectionControllerQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::SelectionController)
    QML_NAMED_ELEMENT(SelectionController)
    QML_UNCREATABLE("Injected by the native application")
};

struct FrameClockQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::FrameClock)
    QML_NAMED_ELEMENT(FrameClock)
    QML_UNCREATABLE("Owned by MotionController")
};
struct MotionControllerQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::MotionController)
    QML_NAMED_ELEMENT(MotionController)
    QML_UNCREATABLE("Injected by the native application")
};

struct LibrarySessionQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::LibrarySession)
    QML_NAMED_ELEMENT(LibrarySession)
    QML_UNCREATABLE("Injected by the native application")
};
struct AlbumListModelQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::AlbumListModel)
    QML_NAMED_ELEMENT(AlbumListModel)
    QML_UNCREATABLE("Owned by LibrarySession")
};
struct SearchListModelQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::SearchListModel)
    QML_NAMED_ELEMENT(SearchListModel)
    QML_UNCREATABLE("Owned by LibrarySession")
};

struct SelectionLightingQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::SelectionLighting)
    QML_NAMED_ELEMENT(SelectionLighting)
    QML_UNCREATABLE("Owned by SceneAdapter")
};
struct RenderQualityQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::RenderQuality)
    QML_NAMED_ELEMENT(RenderQuality)
    QML_UNCREATABLE("Owned by SceneAdapter")
};

struct SceneItemQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::rhi::SceneItem)
    QML_NAMED_ELEMENT(SceneItem)
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

struct OpeningControllerQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::OpeningController)
    QML_NAMED_ELEMENT(OpeningController)
    QML_UNCREATABLE("Owned by application")
};
struct OpeningInkQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::OpeningInk)
    QML_NAMED_ELEMENT(OpeningInk)
};

struct LibraryStoreQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::LibraryStore)
    QML_NAMED_ELEMENT(LibraryStore)
    QML_UNCREATABLE("Owned by application")
};
struct TrackFocusControllerQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::TrackFocusController)
    QML_NAMED_ELEMENT(TrackFocusController)
    QML_UNCREATABLE("Owned by application")
};
struct PanelControllerQml
{
    Q_GADGET
    QML_FOREIGN(yalahoo::PanelController)
    QML_NAMED_ELEMENT(PanelController)
    QML_UNCREATABLE("Owned by application")
};
