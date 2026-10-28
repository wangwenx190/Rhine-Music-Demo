include_guard(GLOBAL)

function(yalahoo_add_branding target)
    qt_add_resources(${target} ${target}_branding PREFIX /assets
        BASE "${PROJECT_SOURCE_DIR}/resources"
        FILES "${PROJECT_SOURCE_DIR}/resources/branding/logo-light.svg"
            "${PROJECT_SOURCE_DIR}/resources/branding/logo-dark.svg")
endfunction()

# The production RHI and isolated material Review read identical baked lighting inputs.
function(yalahoo_environment_assets output)
    set(files)
    foreach(name IN ITEMS room-pmrem dfg ltc1 ltc2)
        list(APPEND files "${PROJECT_SOURCE_DIR}/resources/environment/${name}.rgba32f")
    endforeach()
    set(${output} ${files} PARENT_SCOPE)
endfunction()

function(yalahoo_add_assets target)
    include("${PROJECT_SOURCE_DIR}/resources/manifests/assets.cmake")
    list(APPEND YALAHOO_ASSET_FILES "${PROJECT_SOURCE_DIR}/resources/icons/yalahoo/yalahoo.svg")
    if(target STREQUAL "Yalahoo" AND NOT YALAHOO_REVIEW)
        list(FILTER YALAHOO_ASSET_FILES EXCLUDE REGEX "/models/(meshes/|Music_case_v040\\.qml)")
        list(APPEND YALAHOO_ASSET_FILES
            "${PROJECT_SOURCE_DIR}/resources/licenses/three-LICENSE"
            "${PROJECT_SOURCE_DIR}/resources/licenses/materials-NOTICE.md"
        )
    endif()
    list(APPEND YALAHOO_ASSET_FILES "${PROJECT_SOURCE_DIR}/resources/licenses/qt-playback-reference-NOTICE.md")
    qt_add_resources(
        ${target}
        yalahoo_assets
        PREFIX
        /assets
        BASE
        "${PROJECT_SOURCE_DIR}/resources"
        FILES
        ${YALAHOO_ASSET_FILES}
    )
endfunction()

# Product and UI Review consume the same shader-resource closure.
function(yalahoo_add_ui_shaders target)
    set(files)
    foreach(name IN ITEMS display-output.frag opening-paper.frag opening-blur.frag edge-fade.frag
            reel-slot.frag overview-blur.frag detail-tabs-blur.frag panel-blur.frag panel-shadow.frag ${ARGN})
        list(APPEND files "${PROJECT_SOURCE_DIR}/resources/shaders/${name}")
    endforeach()
    qt_add_shaders(${target} ui_shaders PREFIX /assets/shaders
        BASE "${PROJECT_SOURCE_DIR}/resources/shaders" FILES ${files})
endfunction()
