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
if(YALAHOO_REVIEW)
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
