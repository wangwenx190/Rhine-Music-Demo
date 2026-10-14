qt_add_library(YalahooImageComparison STATIC ImageComparison.cpp ImageComparison.h)
target_link_libraries(YalahooImageComparison PUBLIC Qt6::Gui)
target_include_directories(YalahooImageComparison PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
yalahoo_platform(YalahooImageComparison)
qt_add_executable(compare-images CompareImages.cpp)
target_link_libraries(compare-images PRIVATE YalahooImageComparison)
set_target_properties(
    compare-images
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/tools"
    WIN32_EXECUTABLE
    FALSE
    MACOSX_BUNDLE
    FALSE
)
yalahoo_platform(compare-images)
qt_add_executable(ui-audit UiAudit.cpp RulerAudit.cpp RulerAudit.h CanvasProbe.cpp CanvasProbe.h OffscreenSurface.h
    TitleAudit.cpp TitleAudit.h CanvasTitleProbe.cpp CanvasTitleProbe.h
    "${PROJECT_SOURCE_DIR}/src/app/AssetCatalog.cpp")
target_include_directories(ui-audit PRIVATE "${PROJECT_SOURCE_DIR}/src/app")
target_link_libraries(ui-audit PRIVATE YalahooScene YalahooNavigation YalahooText YalahooImageComparison
    Qt6::CanvasPainter Qt6::QuickControls2 Qt6::Svg Qt6::Test Qt6::GuiPrivate)
set_target_properties(ui-audit PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools"
    WIN32_EXECUTABLE FALSE MACOSX_BUNDLE FALSE)
yalahoo_platform(ui-audit)
include("${PROJECT_SOURCE_DIR}/cmake/Assets.cmake")
yalahoo_add_assets(ui-audit)
qt_add_resources(ui-audit ui_branding PREFIX /assets BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES "${PROJECT_SOURCE_DIR}/resources/branding/logo-light.svg"
    "${PROJECT_SOURCE_DIR}/resources/branding/logo-dark.svg")
include("${PROJECT_SOURCE_DIR}/cmake/BrowseQml.cmake")
yalahoo_browse_qml(ui_qml_files)
foreach(name UiReview Canvas2DProbe Quick2DProbe RulerReview TitleReview)
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/${name}.qml"
        PROPERTIES QT_RESOURCE_ALIAS "${name}.qml")
    list(APPEND ui_qml_files "${CMAKE_CURRENT_SOURCE_DIR}/${name}.qml")
endforeach()
qt_add_qml_module(ui-audit URI Yalahoo.Music VERSION 1.0 RESOURCE_PREFIX /qt/qml
    DEPENDENCIES QtQuick QtCanvas2D
    QML_FILES ${ui_qml_files}
    SOURCES CanvasProbe.h CanvasTitleProbe.h UiTypes.h "${PROJECT_SOURCE_DIR}/src/app/AssetCatalog.h"
    "${PROJECT_SOURCE_DIR}/src/app/ApplicationIdentity.h")
target_compile_definitions(ui-audit PRIVATE FIXTURE_ROOT="${PROJECT_SOURCE_DIR}")
qt_add_shaders(ui-audit navigation_shaders PREFIX /assets/shaders
    BASE "${PROJECT_SOURCE_DIR}/resources/shaders"
    FILES "${PROJECT_SOURCE_DIR}/resources/shaders/edge-fade.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/reel-slot.frag")
if(YALAHOO_REVIEW)
    qt_add_executable(archive-pool-audit ArchivePoolAudit.cpp)
    target_link_libraries(archive-pool-audit PRIVATE YalahooRhi YalahooImageComparison)
    target_include_directories(archive-pool-audit PRIVATE "${PROJECT_SOURCE_DIR}/src/app")
    target_compile_definitions(archive-pool-audit PRIVATE FIXTURE_ROOT="${PROJECT_SOURCE_DIR}")
    set_target_properties(archive-pool-audit PROPERTIES RUNTIME_OUTPUT_DIRECTORY
        "${CMAKE_BINARY_DIR}/tools" WIN32_EXECUTABLE FALSE MACOSX_BUNDLE FALSE)
    yalahoo_platform(archive-pool-audit)
    qt_add_resources(archive-pool-audit pool_covers PREFIX /assets
        BASE "${PROJECT_SOURCE_DIR}/resources" FILES
        "${PROJECT_SOURCE_DIR}/resources/demo-covers/square.png"
        "${PROJECT_SOURCE_DIR}/resources/demo-covers/portrait.png"
        "${PROJECT_SOURCE_DIR}/resources/demo-covers/landscape.png")
    qt_add_executable(cover-audit CoverAudit.cpp)
    target_link_libraries(cover-audit PRIVATE YalahooRhi YalahooImageComparison)
    target_include_directories(cover-audit PRIVATE "${PROJECT_SOURCE_DIR}/src/app")
    set_target_properties(cover-audit PROPERTIES RUNTIME_OUTPUT_DIRECTORY
        "${CMAKE_BINARY_DIR}/tools" WIN32_EXECUTABLE FALSE MACOSX_BUNDLE FALSE)
    yalahoo_platform(cover-audit)
    qt_add_executable(rhi-postprocess-audit RhiPostprocessAudit.cpp RhiReviewInput.h)
    target_link_libraries(rhi-postprocess-audit PRIVATE YalahooRhi YalahooImageComparison)
    target_include_directories(rhi-postprocess-audit PRIVATE "${PROJECT_SOURCE_DIR}/src/app")
    target_compile_definitions(rhi-postprocess-audit PRIVATE FIXTURE_ROOT="${PROJECT_SOURCE_DIR}")
    set_target_properties(rhi-postprocess-audit PROPERTIES RUNTIME_OUTPUT_DIRECTORY
        "${CMAKE_BINARY_DIR}/tools" WIN32_EXECUTABLE FALSE MACOSX_BUNDLE FALSE)
    yalahoo_platform(rhi-postprocess-audit)
    qt_add_resources(rhi-postprocess-audit rhi_post_covers PREFIX /assets
        BASE "${PROJECT_SOURCE_DIR}/resources" FILES
        "${PROJECT_SOURCE_DIR}/resources/demo-covers/square.png"
        "${PROJECT_SOURCE_DIR}/resources/demo-covers/portrait.png"
        "${PROJECT_SOURCE_DIR}/resources/demo-covers/landscape.png")
    qt_add_executable(rhi-shell-audit RhiShellAudit.cpp)
    target_link_libraries(rhi-shell-audit PRIVATE YalahooRhi YalahooImageComparison)
    target_include_directories(rhi-shell-audit PRIVATE "${PROJECT_SOURCE_DIR}/src/app")
    target_compile_definitions(rhi-shell-audit PRIVATE FIXTURE_ROOT="${PROJECT_SOURCE_DIR}")
    set_target_properties(
        rhi-shell-audit
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY
        "${CMAKE_BINARY_DIR}/tools"
        WIN32_EXECUTABLE
        FALSE
        MACOSX_BUNDLE
        FALSE
    )
    yalahoo_platform(rhi-shell-audit)
    qt_add_resources(
        rhi-shell-audit
        rhi_shell_covers
        PREFIX
        /assets
        BASE
        "${PROJECT_SOURCE_DIR}/resources"
        FILES
        "${PROJECT_SOURCE_DIR}/resources/demo-covers/square.png"
        "${PROJECT_SOURCE_DIR}/resources/demo-covers/portrait.png"
        "${PROJECT_SOURCE_DIR}/resources/demo-covers/landscape.png"
    )
    qt_add_executable(
        rhi-audit
        RhiAudit.cpp
        "${PROJECT_SOURCE_DIR}/src/app/FixedTextureData.cpp"
        "${PROJECT_SOURCE_DIR}/src/app/FixedTextureData.h"
    )
    target_include_directories(rhi-audit PRIVATE "${PROJECT_SOURCE_DIR}/src/app")
    target_link_libraries(rhi-audit PRIVATE YalahooRhi YalahooImageComparison Qt6::Quick3D)
    target_compile_definitions(rhi-audit PRIVATE FIXTURE_ROOT="${PROJECT_SOURCE_DIR}")
    set_target_properties(
        rhi-audit
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY
        "${CMAKE_BINARY_DIR}/tools"
        WIN32_EXECUTABLE
        FALSE
        MACOSX_BUNDLE
        FALSE
    )
    yalahoo_platform(rhi-audit)
    qt_add_resources(
        rhi-audit
        rhi_oracle
        PREFIX
        /review
        BASE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        FILES
        RhiOracle.qml
    )
    qt_add_resources(
        rhi-audit
        rhi_oracle_assets
        PREFIX
        /assets
        BASE
        "${PROJECT_SOURCE_DIR}/resources"
        FILES
        "${PROJECT_SOURCE_DIR}/resources/shaders/album-paper.vert"
        "${PROJECT_SOURCE_DIR}/resources/shaders/ao-prepass.frag"
        "${PROJECT_SOURCE_DIR}/resources/shaders/album-paper.frag"
        "${PROJECT_SOURCE_DIR}/resources/models/meshes/meshes_0__mesh.mesh"
        "${PROJECT_SOURCE_DIR}/resources/models/meshes/meshes_1__mesh.mesh"
        "${PROJECT_SOURCE_DIR}/resources/models/meshes/meshes_2__mesh.mesh"
        "${PROJECT_SOURCE_DIR}/resources/environment/room-pmrem.rgba32f"
        "${PROJECT_SOURCE_DIR}/resources/demo-covers/square.png"
        "${PROJECT_SOURCE_DIR}/resources/demo-covers/portrait.png"
        "${PROJECT_SOURCE_DIR}/resources/demo-covers/landscape.png"
    )
    qt_add_executable(render-backend-audit RenderBackendAudit.cpp)
    target_link_libraries(render-backend-audit PRIVATE YalahooImageComparison Qt6::Quick)
    set_target_properties(
        render-backend-audit
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY
        "${CMAKE_BINARY_DIR}/tools"
        WIN32_EXECUTABLE
        FALSE
        MACOSX_BUNDLE
        FALSE
    )
    yalahoo_platform(render-backend-audit)
endif()
