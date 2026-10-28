qt_add_executable(
    Yalahoo
    ApplicationRuntime.cpp
    ApplicationRuntime.h
    ApplicationOptions.cpp
    ApplicationOptions.h
    LibraryPersistence.cpp
    LibraryPersistence.h
    AssetCatalog.cpp
    AssetCatalog.h
)
add_subdirectory("${PROJECT_SOURCE_DIR}/tests/application" "${CMAKE_BINARY_DIR}/tests/application")
include("${PROJECT_SOURCE_DIR}/cmake/Assets.cmake")
yalahoo_add_assets(Yalahoo)
set_target_properties(
    Yalahoo
    PROPERTIES
    OUTPUT_NAME
    yalahoo
    RUNTIME_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/bin"
)
if(WIN32)
    enable_language(RC)
    configure_file("${PROJECT_SOURCE_DIR}/cmake/yalahoo.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/yalahoo.rc" @ONLY)
    target_sources(Yalahoo PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/yalahoo.rc")
    set_target_properties(Yalahoo PROPERTIES WIN32_EXECUTABLE TRUE
        QT_WINDOWS_APP_PROJECT_IDENTIFIER "yalahoo.music")
elseif(APPLE)
    target_sources(Yalahoo PRIVATE "${PROJECT_SOURCE_DIR}/src/platform/macos/ApplicationMenu.mm")
    target_include_directories(Yalahoo PRIVATE "${PROJECT_SOURCE_DIR}/src/platform/macos")
    target_link_libraries(Yalahoo PRIVATE "-framework AppKit")
    set(bundle_icon "${PROJECT_SOURCE_DIR}/resources/icons/yalahoo/yalahoo.icns")
    target_sources(Yalahoo PRIVATE "${bundle_icon}")
    set_source_files_properties("${bundle_icon}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(Yalahoo PROPERTIES MACOSX_BUNDLE_ICON_FILE yalahoo.icns)
    set_target_properties(
        Yalahoo
        PROPERTIES
        MACOSX_BUNDLE
        TRUE
        MACOSX_BUNDLE_BUNDLE_NAME
        "yalahOO!!!"
        MACOSX_BUNDLE_GUI_IDENTIFIER
        "yalahoo.music"
        MACOSX_BUNDLE_INFO_PLIST
        "${PROJECT_SOURCE_DIR}/cmake/Info.plist.in"
    )
elseif(UNIX)
    set_target_properties(Yalahoo PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
endif()
include("${PROJECT_SOURCE_DIR}/cmake/BrowseQml.cmake")
yalahoo_product_qml(product_qml_files)
yalahoo_add_branding(Yalahoo)
file(GLOB application_icons "${PROJECT_SOURCE_DIR}/resources/icons/yalahoo/png/yalahoo-*.png")
qt_add_resources(Yalahoo application_icons PREFIX /assets BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES ${application_icons})
qt_add_qml_module(Yalahoo URI Yalahoo.Music VERSION 1.0
    DEPENDENCIES QtQuick ${review_dependencies}
    RESOURCE_PREFIX /qt/qml
    QML_FILES ${product_qml_files} ${review_qml_files}
    SOURCES QmlTypes.h ApplicationIdentity.h ${review_sources})
yalahoo_add_ui_shaders(Yalahoo)
target_link_libraries(
    Yalahoo
    PRIVATE
    YalahooCore
    YalahooScene
    YalahooRhi
    YalahooNavigation
    YalahooText
    YalahooVisuals
    YalahooOpening
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickVectorImage
    Qt6::QuickTemplates2
    Qt6::QuickDialogs2
    Qt6::Multimedia
    Qt6::Concurrent
    Qt6::Svg
    Qt6::ShaderTools
    Qt6::CanvasPainter
)
yalahoo_platform(Yalahoo)
install(TARGETS Yalahoo BUNDLE DESTINATION . RUNTIME DESTINATION bin)

include("${PROJECT_SOURCE_DIR}/cmake/Deployment.cmake")
