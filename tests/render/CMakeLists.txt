include("${PROJECT_SOURCE_DIR}/cmake/ProbeQml.cmake")
qt_add_executable(ui-tests UiTests.cpp StartupChecks.cpp ControlsChecks.cpp ExperimentChecks.cpp
    LibraryChecks.cpp OpeningChecks.cpp OverviewChecks.cpp PresentationChecks.cpp)
target_link_libraries(ui-tests PRIVATE Qt6::Test)
yalahoo_test_tool(ui-tests)
yalahoo_probe_qml(ui-tests
    DetailReview.qml OverviewVisual.qml LibraryReview.qml ControlsReview.qml)

# Native RHI probes share executable policy; only real dependencies vary below.
function(yalahoo_render_test target)
    qt_add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE YalahooRhi)
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src/app" "${PROJECT_SOURCE_DIR}/tests/support")
    yalahoo_test_tool(${target})
endfunction()

if(YALAHOO_REVIEW)
    yalahoo_render_test(archive-pool-test ArchivePoolChecks.cpp "${PROJECT_SOURCE_DIR}/tests/support/OffscreenSurface.h")
    yalahoo_render_test(cover-test CoverChecks.cpp)
    yalahoo_render_test(rhi-postprocess-test RhiPostprocessChecks.cpp RhiReviewInput.h RhiCaptureHarness.h "${PROJECT_SOURCE_DIR}/tests/support/OffscreenSurface.h")
    yalahoo_render_test(rhi-bokeh-test RhiBokehChecks.cpp OffscreenRhi.h ImageSampling.h)
    yalahoo_render_test(rhi-smaa-test RhiSmaaChecks.cpp OffscreenRhi.h SmaaReference.h ImageSampling.h)
    yalahoo_render_test(rhi-ao-test RhiAoChecks.cpp RhiCaptureHarness.h "${PROJECT_SOURCE_DIR}/tests/support/OffscreenSurface.h")
    yalahoo_render_test(rhi-fog-test RhiFogChecks.cpp RhiFogChecks.h RhiCaptureHarness.h "${PROJECT_SOURCE_DIR}/tests/support/OffscreenSurface.h")
    yalahoo_render_test(rhi-shadow-test RhiShadowChecks.cpp RhiShadowChecks.h RhiCaptureHarness.h "${PROJECT_SOURCE_DIR}/tests/support/OffscreenSurface.h")
    yalahoo_render_test(rhi-flicker-test RhiFlickerChecks.cpp RhiCaptureHarness.h "${PROJECT_SOURCE_DIR}/tests/support/OffscreenSurface.h")
    yalahoo_render_test(rhi-scale-test RhiScaleChecks.cpp RhiCaptureHarness.h "${PROJECT_SOURCE_DIR}/tests/support/OffscreenSurface.h")
    yalahoo_render_test(rhi-shell-test RhiShellChecks.cpp RhiCaptureHarness.h "${PROJECT_SOURCE_DIR}/tests/support/OffscreenSurface.h")
    yalahoo_render_test(rhi-opaque-test RhiOpaqueChecks.cpp RhiCaptureHarness.h "${PROJECT_SOURCE_DIR}/tests/support/OffscreenSurface.h")
    set(rhi_fixture_targets archive-pool-test rhi-postprocess-test rhi-bokeh-test
        rhi-smaa-test rhi-ao-test rhi-fog-test rhi-shadow-test rhi-flicker-test
        rhi-shell-test)
    foreach(target IN LISTS rhi_fixture_targets)
        target_compile_definitions(${target} PRIVATE FIXTURE_ROOT="${PROJECT_SOURCE_DIR}")
    endforeach()
    foreach(target IN ITEMS ${rhi_fixture_targets} rhi-scale-test rhi-opaque-test)
        target_link_libraries(${target} PRIVATE Qt6::GuiPrivate)
    endforeach()
    foreach(target IN ITEMS rhi-bokeh-test rhi-smaa-test rhi-shadow-test rhi-scale-test)
        target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/tools/assets")
    endforeach()
    foreach(target IN ITEMS archive-pool-test rhi-postprocess-test rhi-ao-test
            rhi-fog-test rhi-shadow-test rhi-flicker-test rhi-scale-test rhi-shell-test rhi-opaque-test)
        qt_add_resources(${target} review_covers PREFIX /assets
            BASE "${PROJECT_SOURCE_DIR}/resources" FILES
            "${PROJECT_SOURCE_DIR}/resources/demo-covers/square.png"
            "${PROJECT_SOURCE_DIR}/resources/demo-covers/portrait.png"
            "${PROJECT_SOURCE_DIR}/resources/demo-covers/landscape.png")
    endforeach()
endif()
