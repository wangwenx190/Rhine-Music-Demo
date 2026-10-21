qt_add_executable(ui-audit UiAudit.cpp StartupAudit.cpp StartupAudit.h ControlsAudit.cpp ControlsAudit.h ExperimentAudit.cpp ExperimentAudit.h LibraryAudit.cpp LibraryAudit.h OpeningAudit.cpp OpeningAudit.h OverviewAudit.cpp OverviewAudit.h PresentationAudit.cpp PresentationAudit.h RulerAudit.cpp RulerAudit.h CanvasProbe.cpp CanvasProbe.h OffscreenSurface.h
    TitleAudit.cpp TitleAudit.h CanvasTitleProbe.cpp CanvasTitleProbe.h
    "${PROJECT_SOURCE_DIR}/src/app/AssetCatalog.cpp")
target_include_directories(ui-audit PRIVATE "${PROJECT_SOURCE_DIR}/src/app" "${PROJECT_SOURCE_DIR}/tests/support")
target_link_libraries(ui-audit PRIVATE YalahooScene YalahooNavigation YalahooText YalahooOpening YalahooRhi YalahooVisuals
    Qt6::CanvasPainter Qt6::QuickTemplates2 Qt6::QuickVectorImage Qt6::Svg Qt6::Test Qt6::GuiPrivate)
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
foreach(name UiReview Canvas2DProbe Quick2DProbe RulerReview TitleReview DetailReview OverviewVisual OpeningComparison LibraryReview ControlsReview)
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/${name}.qml"
        PROPERTIES QT_RESOURCE_ALIAS "${name}.qml")
    list(APPEND ui_qml_files "${CMAKE_CURRENT_SOURCE_DIR}/${name}.qml")
endforeach()
set(settings_app "${PROJECT_SOURCE_DIR}/qml/Yalahoo/Music/App.qml")
set_source_files_properties("${settings_app}" PROPERTIES QT_RESOURCE_ALIAS App.qml)
list(APPEND ui_qml_files "${settings_app}")
qt_add_qml_module(ui-audit URI Yalahoo.Music VERSION 1.0 RESOURCE_PREFIX /qt/qml
    DEPENDENCIES QtQuick QtCanvas2D
    QML_FILES ${ui_qml_files}
    SOURCES CanvasProbe.h CanvasTitleProbe.h "${PROJECT_SOURCE_DIR}/src/app/QmlTypes.h" "${PROJECT_SOURCE_DIR}/src/app/AssetCatalog.h"
    "${PROJECT_SOURCE_DIR}/src/app/ApplicationIdentity.h")
target_compile_definitions(ui-audit PRIVATE FIXTURE_ROOT="${PROJECT_SOURCE_DIR}")
yalahoo_add_ui_shaders(ui-audit)
# Native RHI probes share executable policy; only real dependencies vary below.
function(yalahoo_rhi_review target)
    qt_add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE YalahooRhi)
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src/app")
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY
        "${CMAKE_BINARY_DIR}/tools" WIN32_EXECUTABLE FALSE MACOSX_BUNDLE FALSE)
    yalahoo_platform(${target})
endfunction()

if(YALAHOO_REVIEW)
    yalahoo_rhi_review(archive-pool-audit ArchivePoolAudit.cpp OffscreenSurface.h)
    yalahoo_rhi_review(cover-audit CoverAudit.cpp)
    yalahoo_rhi_review(rhi-postprocess-audit RhiPostprocessAudit.cpp RhiReviewInput.h RhiCaptureHarness.h OffscreenSurface.h)
    yalahoo_rhi_review(rhi-bokeh-audit RhiBokehAudit.cpp OffscreenRhi.h ImageSampling.h)
    yalahoo_rhi_review(rhi-smaa-audit RhiSmaaAudit.cpp OffscreenRhi.h SmaaReference.h ImageSampling.h)
    yalahoo_rhi_review(rhi-ao-audit RhiAoAudit.cpp RhiCaptureHarness.h OffscreenSurface.h)
    yalahoo_rhi_review(rhi-fog-audit RhiFogAudit.cpp RhiFogAudit.h RhiCaptureHarness.h OffscreenSurface.h)
    yalahoo_rhi_review(rhi-shadow-audit RhiShadowAudit.cpp RhiShadowAudit.h RhiCaptureHarness.h OffscreenSurface.h)
    yalahoo_rhi_review(rhi-flicker-audit RhiFlickerAudit.cpp RhiCaptureHarness.h OffscreenSurface.h)
    yalahoo_rhi_review(rhi-scale-audit RhiScaleAudit.cpp RhiCaptureHarness.h OffscreenSurface.h)
    yalahoo_rhi_review(rhi-shell-audit RhiShellAudit.cpp RhiCaptureHarness.h OffscreenSurface.h)
    yalahoo_rhi_review(rhi-audit RhiAudit.cpp RhiCaptureHarness.h OffscreenSurface.h)
    set(rhi_fixture_targets archive-pool-audit rhi-postprocess-audit rhi-bokeh-audit
        rhi-smaa-audit rhi-ao-audit rhi-fog-audit rhi-shadow-audit rhi-flicker-audit
        rhi-scale-audit rhi-shell-audit)
    foreach(target IN LISTS rhi_fixture_targets)
        target_compile_definitions(${target} PRIVATE FIXTURE_ROOT="${PROJECT_SOURCE_DIR}")
    endforeach()
    foreach(target IN ITEMS ${rhi_fixture_targets} rhi-audit)
        target_link_libraries(${target} PRIVATE Qt6::GuiPrivate)
    endforeach()
    foreach(target IN ITEMS rhi-bokeh-audit rhi-smaa-audit rhi-shadow-audit rhi-scale-audit)
        target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/tools/assets")
    endforeach()
    foreach(target IN ITEMS archive-pool-audit rhi-postprocess-audit rhi-ao-audit
            rhi-fog-audit rhi-shadow-audit rhi-flicker-audit rhi-scale-audit rhi-shell-audit rhi-audit)
        qt_add_resources(${target} review_covers PREFIX /assets
            BASE "${PROJECT_SOURCE_DIR}/resources" FILES
            "${PROJECT_SOURCE_DIR}/resources/demo-covers/square.png"
            "${PROJECT_SOURCE_DIR}/resources/demo-covers/portrait.png"
            "${PROJECT_SOURCE_DIR}/resources/demo-covers/landscape.png")
    endforeach()
endif()

qt_add_executable(metadata-network-probe MetadataNetworkProbe.cpp)
target_link_libraries(metadata-network-probe PRIVATE YalahooCore Qt6::Network)
set_target_properties(metadata-network-probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools"
    WIN32_EXECUTABLE FALSE MACOSX_BUNDLE FALSE)
yalahoo_platform(metadata-network-probe)
