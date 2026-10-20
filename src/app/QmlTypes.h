// Registers the C++ types and foreign Qt types used by the QML module. Registration exposes
// narrow typed interfaces without giving QML ownership of services.

#pragma once
#include "OptionMetrics.h"
#include "GenreRowsModel.h"
#include "OpeningController.h"
#include "StartupCoordinator.h"
#include "OpeningInk.h"
#include "TextAreaFormat.h"
#include "SceneGraphLifecycle.h"
#include "LibrarySession.h"
#include "SettingsController.h"
#include "PlaybackController.h"
#include "BackgroundMusicController.h"
#include "InterfaceSoundController.h"
#include "LibraryStore.h"
#include "TrackFocusController.h"
#include "PanelController.h"
#include "PresentationController.h"
#include "DetailModel.h"
#include "MotionController.h"
#include "SceneAdapter.h"
#include "OverviewController.h"
#include "SceneItem.h"
#include "DisplayOutput.h"
#include "RulerController.h"
#include "RulerInk.h"
#include "InputRouter.h"
#include "OrdinalMotion.h"
#include "TitleReel.h"
#include "ReelInk.h"
#include "TransportMotion.h"
#include <QCanvasPainterItem>
#include <QtQml/qqmlregistration.h>

struct DisplayOutputQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::DisplayOutput)
    QML_NAMED_ELEMENT(DisplayOutput)
    QML_UNCREATABLE("Display output is owned by the application")
};

struct LayoutMetricsQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::LayoutMetrics)
    QML_NAMED_ELEMENT(LayoutMetrics)
    QML_UNCREATABLE("Owned by SceneAdapter")
};
struct SceneAdapterQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::SceneAdapter)
    QML_NAMED_ELEMENT(SceneAdapter)
    QML_UNCREATABLE("Injected by the native application")
};
struct SelectionControllerQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::SelectionController)
    QML_NAMED_ELEMENT(SelectionController)
    QML_UNCREATABLE("Injected by the native application")
};

struct FrameClockQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::FrameClock)
    QML_NAMED_ELEMENT(FrameClock)
    QML_UNCREATABLE("Owned by MotionController")
};
struct MotionControllerQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::MotionController)
    QML_NAMED_ELEMENT(MotionController)
    QML_UNCREATABLE("Injected by the native application")
};

struct LibrarySessionQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::LibrarySession)
    QML_NAMED_ELEMENT(LibrarySession)
    QML_UNCREATABLE("Injected by the native application")
};
struct AlbumListModelQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::AlbumListModel)
    QML_NAMED_ELEMENT(AlbumListModel)
    QML_UNCREATABLE("Owned by LibrarySession")
};
struct SearchListModelQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::SearchListModel)
    QML_NAMED_ELEMENT(SearchListModel)
    QML_UNCREATABLE("Owned by LibrarySession")
};

struct SelectionLightingQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::SelectionLighting)
    QML_NAMED_ELEMENT(SelectionLighting)
    QML_UNCREATABLE("Owned by SceneAdapter")
};
struct RenderQualityQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::RenderQuality)
    QML_NAMED_ELEMENT(RenderQuality)
    QML_UNCREATABLE("Owned by SceneAdapter")
};

struct SceneItemQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::rhi::SceneItem)
    QML_NAMED_ELEMENT(SceneItem)
};

struct RulerControllerQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::RulerController)
    QML_NAMED_ELEMENT(RulerController)
    QML_UNCREATABLE("Owned by application")
};
struct InputRouterQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::InputRouter)
    QML_NAMED_ELEMENT(InputRouter)
    QML_UNCREATABLE("Owned by application")
};
struct RulerInkQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::RulerInk)
    QML_NAMED_ELEMENT(RulerInk)
};
struct OrdinalMotionQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::OrdinalMotion)
    QML_NAMED_ELEMENT(OrdinalMotion)
};
struct CanvasPainterBase final
{
    Q_GADGET
    QML_FOREIGN(QCanvasPainterItem)
    QML_ANONYMOUS
};
struct TitleReelQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::TitleReel)
    QML_NAMED_ELEMENT(TitleReelMotion)
};
struct ReelSlotQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::ReelSlot)
    QML_NAMED_ELEMENT(ReelSlot)
    QML_UNCREATABLE("Owned by TitleReelMotion")
};
struct ReelInkQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::ReelInk)
    QML_NAMED_ELEMENT(ReelInk)
};
struct TransportMotionQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::TransportMotion)
    QML_NAMED_ELEMENT(TransportMotion)
};

struct PresentationControllerQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::PresentationController)
    QML_NAMED_ELEMENT(PresentationController)
    QML_UNCREATABLE("Owned by application")
};
struct SurfaceTransitionQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::SurfaceTransition)
    QML_NAMED_ELEMENT(SurfaceTransition)
    QML_UNCREATABLE("Owned by PresentationController")
};
struct DetailModelQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::DetailModel)
    QML_NAMED_ELEMENT(DetailModel)
    QML_UNCREATABLE("Owned by application")
};

struct OverviewControllerQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::OverviewController)
    QML_NAMED_ELEMENT(OverviewController)
    QML_UNCREATABLE("Owned by application")
};
struct OverviewLabelsModelQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::OverviewLabelsModel)
    QML_NAMED_ELEMENT(OverviewLabelsModel)
    QML_UNCREATABLE("Owned by OverviewController")
};

struct OpeningControllerQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::OpeningController)
    QML_NAMED_ELEMENT(OpeningController)
    QML_UNCREATABLE("Owned by application")
};
struct OpeningInkQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::OpeningInk)
    QML_NAMED_ELEMENT(OpeningInk)
};

struct LibraryStoreQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::LibraryStore)
    QML_NAMED_ELEMENT(LibraryStore)
    QML_UNCREATABLE("Owned by application")
};
struct TrackFocusControllerQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::TrackFocusController)
    QML_NAMED_ELEMENT(TrackFocusController)
    QML_UNCREATABLE("Owned by application")
};
struct PanelControllerQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::PanelController)
    QML_NAMED_ELEMENT(PanelController)
    QML_UNCREATABLE("Owned by application")
};

struct PlaybackControllerQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::PlaybackController)
    QML_NAMED_ELEMENT(PlaybackController)
    QML_UNCREATABLE("Owned by application")
};

struct BackgroundMusicControllerQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::BackgroundMusicController)
    QML_NAMED_ELEMENT(BackgroundMusicController)
    QML_UNCREATABLE("Owned by application")
};

struct InterfaceSoundControllerQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::InterfaceSoundController)
    QML_NAMED_ELEMENT(InterfaceSoundController)
    QML_UNCREATABLE("Owned by application")
};

struct SettingsControllerQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::SettingsController)
    QML_NAMED_ELEMENT(SettingsController)
    QML_UNCREATABLE("Owned by application")
};

struct TextAreaFormatQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::TextAreaFormat)
    QML_NAMED_ELEMENT(TextAreaFormat)
};

struct StartupCoordinatorQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::StartupCoordinator)
    QML_NAMED_ELEMENT(StartupCoordinator)
    QML_UNCREATABLE("Owned by application")
};

struct GenreRowsModelQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::GenreRowsModel)
    QML_NAMED_ELEMENT(GenreRowsModel)
};

struct OptionMetricsQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::OptionMetrics)
    QML_NAMED_ELEMENT(OptionMetrics)
};

struct SceneGraphLifecycleQml final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::SceneGraphLifecycle)
    QML_NAMED_ELEMENT(SceneGraphLifecycle)
};
