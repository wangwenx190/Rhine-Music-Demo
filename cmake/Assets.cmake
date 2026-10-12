function(yalahoo_add_assets target)
    include("${PROJECT_SOURCE_DIR}/resources/manifests/assets.cmake")
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
