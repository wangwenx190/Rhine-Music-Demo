qt_add_library(
    YalahooScene
    STATIC
    LayoutMetrics.cpp
    LayoutMetrics.h
    SceneAdapter.cpp
    SceneAdapter.h
    SceneFrame.h
    ArchivePool.cpp
    ArchivePool.h
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
    RenderQuality.cpp
    RenderQuality.h
)
target_link_libraries(YalahooScene PUBLIC YalahooCore Qt6::Gui)
target_include_directories(YalahooScene PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(YalahooScene PUBLIC cxx_std_23)
yalahoo_platform(YalahooScene)
add_subdirectory(rhi)
