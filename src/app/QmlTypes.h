#pragma once
#include "LibrarySession.h"
#include "MotionController.h"
#include "SceneAdapter.h"
#include <QtQml/qqmlregistration.h>

struct LayoutMetricsQml {
    Q_GADGET
    QML_FOREIGN(yalahoo::LayoutMetrics)
    QML_NAMED_ELEMENT(LayoutMetrics)
    QML_UNCREATABLE("Owned by SceneAdapter")
};
struct SceneAdapterQml {
    Q_GADGET
    QML_FOREIGN(yalahoo::SceneAdapter)
    QML_NAMED_ELEMENT(SceneAdapter)
    QML_UNCREATABLE("Injected by the native application")
};

struct FrameClockQml {
    Q_GADGET
    QML_FOREIGN(yalahoo::FrameClock)
    QML_NAMED_ELEMENT(FrameClock)
    QML_UNCREATABLE("Owned by MotionController")
};
struct MotionControllerQml {
    Q_GADGET
    QML_FOREIGN(yalahoo::MotionController)
    QML_NAMED_ELEMENT(MotionController)
    QML_UNCREATABLE("Injected by the native application")
};

struct LibrarySessionQml {
    Q_GADGET
    QML_FOREIGN(yalahoo::LibrarySession)
    QML_NAMED_ELEMENT(LibrarySession)
    QML_UNCREATABLE("Injected by the native application")
};
struct AlbumListModelQml {
    Q_GADGET
    QML_FOREIGN(yalahoo::AlbumListModel)
    QML_NAMED_ELEMENT(AlbumListModel)
    QML_UNCREATABLE("Owned by LibrarySession")
};
struct SearchListModelQml {
    Q_GADGET
    QML_FOREIGN(yalahoo::SearchListModel)
    QML_NAMED_ELEMENT(SearchListModel)
    QML_UNCREATABLE("Owned by LibrarySession")
};

struct SelectionLightingQml {
    Q_GADGET
    QML_FOREIGN(yalahoo::SelectionLighting)
    QML_NAMED_ELEMENT(SelectionLighting)
    QML_UNCREATABLE("Owned by SceneAdapter")
};
struct RenderQualityQml {
    Q_GADGET
    QML_FOREIGN(yalahoo::RenderQuality)
    QML_NAMED_ELEMENT(RenderQuality)
    QML_UNCREATABLE("Owned by SceneAdapter")
};
