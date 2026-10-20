qt_add_library(
    YalahooScene
    STATIC
    StartupCoordinator.cpp
    StartupCoordinator.h
    LayoutMetrics.cpp
    LayoutMetrics.h
    OverviewController.cpp
    OverviewController.h
    OverviewLabelsModel.cpp
    OverviewLabelsModel.h
    SceneAdapter.cpp
    SceneAdapter.h
    SceneFrame.h
    SceneGeometry.h
    opening/OpeningFrame.cpp
    opening/OpeningFrame.h
    opening/OpeningController.cpp
    opening/OpeningController.h
    ArchivePool.cpp
    ArchivePool.h
    CaseClarity.cpp
    CaseClarity.h
    DetailPorts.cpp
    DetailPorts.h
    DetailMotion.cpp
    DetailMotion.h
    PointerDragController.cpp
    PointerDragController.h
    ReturningCasePool.cpp
    ReturningCasePool.h
    SelectionController.cpp
    SelectionController.h
    RealColumns.cpp
    RealColumns.h
    ColumnMemory.cpp
    ColumnMemory.h
    RowLightController.cpp
    RowLightController.h
    SelectionLighting.cpp
    SelectionLighting.h
    SettingsController.cpp
    SettingsController.h
    RenderQuality.cpp
    RenderQuality.h
)
target_link_libraries(YalahooScene PUBLIC YalahooCore Qt6::Gui)
target_include_directories(YalahooScene PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(YalahooScene PUBLIC cxx_std_23)
yalahoo_platform(YalahooScene)
add_subdirectory(navigation)
add_subdirectory(text)
add_subdirectory(rhi)
qt_add_library(YalahooOpening STATIC opening/OpeningInk.cpp opening/OpeningInk.h opening/BrandPaths.h)
target_include_directories(YalahooOpening PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/opening")
target_link_libraries(YalahooOpening PUBLIC YalahooScene Qt6::Quick Qt6::CanvasPainter)
yalahoo_platform(YalahooOpening)
qt_add_resources(YalahooOpening opening_brand PREFIX /assets BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES "${PROJECT_SOURCE_DIR}/resources/branding/logo-light.svg"
    "${PROJECT_SOURCE_DIR}/resources/branding/reveal-map.json")

qt_add_library(YalahooVisuals STATIC SceneGraphLifecycle.cpp SceneGraphLifecycle.h)
target_include_directories(YalahooVisuals PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(YalahooVisuals PUBLIC Qt6::Quick)
target_compile_features(YalahooVisuals PUBLIC cxx_std_23)
yalahoo_platform(YalahooVisuals)
