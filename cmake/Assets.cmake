function(yalahoo_add_assets target)
    include("${PROJECT_SOURCE_DIR}/resources/manifests/assets.cmake")
    if(target STREQUAL "Yalahoo" AND NOT YALAHOO_REVIEW)
        list(FILTER YALAHOO_ASSET_FILES EXCLUDE REGEX "/models/(meshes/|Music_case_v040\\.qml)")
        list(APPEND YALAHOO_ASSET_FILES
            "${PROJECT_SOURCE_DIR}/resources/licenses/three-LICENSE"
            "${PROJECT_SOURCE_DIR}/resources/licenses/materials-NOTICE.md"
        )
    endif()
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
