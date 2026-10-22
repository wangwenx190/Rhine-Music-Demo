include_guard(GLOBAL)
include("${PROJECT_SOURCE_DIR}/cmake/Assets.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/BrowseQml.cmake")

# Both hosts use production objects/resources; each target owns its own probe entry points.
function(yalahoo_probe_qml target)
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src/app"
        "${PROJECT_SOURCE_DIR}/tests/support")
    target_link_libraries(${target} PRIVATE YalahooScene YalahooNavigation YalahooText
        YalahooOpening YalahooRhi YalahooVisuals Qt6::CanvasPainter Qt6::QuickTemplates2
        Qt6::QuickVectorImage Qt6::Svg Qt6::GuiPrivate)
    target_compile_definitions(${target} PRIVATE FIXTURE_ROOT="${PROJECT_SOURCE_DIR}")
    target_sources(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src/app/AssetCatalog.cpp"
        "${PROJECT_SOURCE_DIR}/tests/support/OffscreenSurface.h")
    yalahoo_add_assets(${target})
    qt_add_resources(${target} probe_branding PREFIX /assets BASE "${PROJECT_SOURCE_DIR}/resources"
        FILES "${PROJECT_SOURCE_DIR}/resources/branding/logo-light.svg"
            "${PROJECT_SOURCE_DIR}/resources/branding/logo-dark.svg")
    yalahoo_browse_qml(qml_files)
    foreach(file IN LISTS ARGN)
        get_filename_component(name "${file}" NAME)
        set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/${file}"
            PROPERTIES QT_RESOURCE_ALIAS "${name}")
        list(APPEND qml_files "${CMAKE_CURRENT_SOURCE_DIR}/${file}")
    endforeach()
    set(app "${PROJECT_SOURCE_DIR}/qml/Yalahoo/Music/App.qml")
    set_source_files_properties("${app}" PROPERTIES QT_RESOURCE_ALIAS App.qml)
    list(APPEND qml_files "${app}")
    qt_add_qml_module(${target} URI Yalahoo.Music VERSION 1.0 RESOURCE_PREFIX /qt/qml
        DEPENDENCIES QtQuick QML_FILES ${qml_files}
        SOURCES "${PROJECT_SOURCE_DIR}/src/app/QmlTypes.h"
            "${PROJECT_SOURCE_DIR}/src/app/AssetCatalog.h"
            "${PROJECT_SOURCE_DIR}/src/app/ApplicationIdentity.h")
    yalahoo_add_ui_shaders(${target})
endfunction()
