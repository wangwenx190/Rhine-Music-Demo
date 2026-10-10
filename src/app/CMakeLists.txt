qt_add_executable(Yalahoo main.cpp AssetCatalog.cpp AssetCatalog.h)
include("${PROJECT_SOURCE_DIR}/cmake/Assets.cmake")
yalahoo_add_assets(Yalahoo)
set_target_properties(Yalahoo PROPERTIES OUTPUT_NAME yalahoo
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
if(WIN32)
    set_target_properties(Yalahoo PROPERTIES WIN32_EXECUTABLE TRUE)
elseif(APPLE)
    set_target_properties(Yalahoo PROPERTIES MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "yalahOO!!!"
        MACOSX_BUNDLE_GUI_IDENTIFIER "yalahoo.music"
        MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/cmake/Info.plist.in")
elseif(UNIX)
    set_target_properties(Yalahoo PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
endif()
set(app_qml "${PROJECT_SOURCE_DIR}/qml/Yalahoo/Music/App.qml")
set_source_files_properties("${app_qml}" PROPERTIES QT_RESOURCE_ALIAS App.qml)
qt_add_qml_module(Yalahoo URI Yalahoo.Music VERSION 1.0
    RESOURCE_PREFIX /qt/qml QML_FILES "${app_qml}" SOURCES QmlTypes.h)
qt_add_shaders(Yalahoo r02_shader_probe PREFIX /assets/shaders BASE "${PROJECT_SOURCE_DIR}/resources/shaders"
    FILES "${PROJECT_SOURCE_DIR}/resources/shaders/probe.frag")
target_link_libraries(Yalahoo PRIVATE YalahooCore Qt6::Gui Qt6::Qml Qt6::Quick
    Qt6::QuickControls2 Qt6::QuickDialogs2 Qt6::Quick3D Qt6::Multimedia
    Qt6::Concurrent Qt6::Svg Qt6::ShaderTools)
yalahoo_platform(Yalahoo)
install(TARGETS Yalahoo BUNDLE DESTINATION . RUNTIME DESTINATION bin)
