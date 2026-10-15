# Classify actual test entry points. GPU/window checks remain serial and opt-in.
# OffscreenSurface currently uses RenderControl on macOS; other native ports show a window.
set(yalahoo_render_integration_tests ui-drawing presentation-qml overview-qml
    archive-pool-gpu real-columns-gpu runtime-materials runtime-lighting rhi-opaque-gpu rhi-shell-gpu rhi-flicker-gpu rhi-ao-gpu rhi-fog-gpu rhi-shadow-gpu)
set(yalahoo_legacy_image_gates
    render-backend-gpu-audit
    postprocess-gpu-audit
    occlusion-gpu-audit
    rhi-postprocess-gpu
)
foreach(directory IN ITEMS "${PROJECT_SOURCE_DIR}" tests/unit tests/integration)
    get_property(native_tests DIRECTORY "${directory}" PROPERTY TESTS)
    foreach(test IN LISTS native_tests)
        set(needs_native_window FALSE)
        if(test IN_LIST yalahoo_render_integration_tests)
            set_property(TEST "${test}" DIRECTORY "${directory}" APPEND PROPERTY LABELS render-integration)
            set_property(TEST "${test}" DIRECTORY "${directory}" PROPERTY RUN_SERIAL TRUE)
            if(APPLE)
                set_property(TEST "${test}" DIRECTORY "${directory}" APPEND PROPERTY LABELS hidden-render)
                set_property(TEST "${test}" DIRECTORY "${directory}" APPEND PROPERTY ENVIRONMENT
                    "YALAHOO_VERIFY_VISIBLE=0" "YALAHOO_VERIFY_FOREGROUND=0")
            else()
                set(needs_native_window TRUE)
            endif()
        elseif(test MATCHES "^runtime|.*-gpu$|^(verification-window-visible|render-backend-gpu-audit|postprocess-gpu-audit|occlusion-gpu-audit)$")
            set(needs_native_window TRUE)
        else()
            set_property(TEST "${test}" DIRECTORY "${directory}" APPEND PROPERTY LABELS fast)
        endif()
        if(test IN_LIST yalahoo_legacy_image_gates)
            # These entries couple real checks to obsolete source-image gates. R19A–H
            # separate them before reuse; no preset executes their pixel comparisons.
            set_property(TEST "${test}" DIRECTORY "${directory}" APPEND PROPERTY LABELS legacy-image-gate)
            set_property(TEST "${test}" DIRECTORY "${directory}" PROPERTY DISABLED TRUE)
        endif()
        if(needs_native_window)
            set_property(TEST "${test}" DIRECTORY "${directory}" APPEND PROPERTY LABELS native-visible)
            set_property(TEST "${test}" DIRECTORY "${directory}" PROPERTY RUN_SERIAL TRUE)
            if(APPLE AND NOT YALAHOO_MACOS_VISIBLE_TESTS)
                set_property(TEST "${test}" DIRECTORY "${directory}" PROPERTY DISABLED TRUE)
                set_property(TEST "${test}" DIRECTORY "${directory}" APPEND PROPERTY LABELS macos-visible-deferred)
            endif()
        endif()
        if(APPLE)
            set_property(TEST "${test}" DIRECTORY "${directory}" APPEND PROPERTY ENVIRONMENT
                "QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM=1")
        endif()
    endforeach()
endforeach()
