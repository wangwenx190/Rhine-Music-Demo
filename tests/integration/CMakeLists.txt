find_package(Python3 REQUIRED COMPONENTS Interpreter)
yalahoo_unit_test(AssetTest assets
    SOURCES "${PROJECT_SOURCE_DIR}/audit/assets/GeometryAudit.cpp"
        "${PROJECT_SOURCE_DIR}/tools/assets/AssetFiles.cpp"
        "${PROJECT_SOURCE_DIR}/src/app/AssetCatalog.cpp"
    INCLUDES "${PROJECT_SOURCE_DIR}/tools/assets" "${PROJECT_SOURCE_DIR}/audit/assets" "${PROJECT_SOURCE_DIR}/src/app"
    LIBRARIES YalahooCore Qt6::Gui Qt6::Quick3D Qt6::Qml Qt6::Svg Qt6::Multimedia
    TIMEOUT 90
    ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins")
include("${PROJECT_SOURCE_DIR}/cmake/Assets.cmake")
yalahoo_add_assets(AssetTest)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/verification")
yalahoo_unit_test(LibraryQmlTest library-qml
    LIBRARIES YalahooCore Qt6::Qml Qt6::Quick
    DEFINITIONS FIXTURE_ROOT="${PROJECT_SOURCE_DIR}"
    TIMEOUT 30
    ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins")
add_test(
    NAME
    branding
    COMMAND
    "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/tools/BrandingExportTest.py"
    "$<TARGET_FILE:export-branding>" "${PROJECT_SOURCE_DIR}"
    "${CMAKE_BINARY_DIR}/verification"
)
set_tests_properties(
    branding
    PROPERTIES
    TIMEOUT
    90
    ENVIRONMENT
    "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins"
)

yalahoo_ui_test(presentation-qml --presentation presentation 90)

add_test(NAME runtime-detail COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tests/runners/fresh-output.py" "$<TARGET_FILE:Yalahoo>"
    "${CMAKE_BINARY_DIR}/verification/detail-product" --verify-detail "{output}")
set_tests_properties(runtime-detail PROPERTIES TIMEOUT 150
    ENVIRONMENT_MODIFICATION "QSG_NO_VSYNC=unset:"
    ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1"
    FAIL_REGULAR_EXPRESSION "Binding loop;Cannot assign;ReferenceError;TypeError;Failed to compile")

add_test(NAME runtime-overview COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tests/runners/fresh-output.py" "$<TARGET_FILE:Yalahoo>"
    "${CMAKE_BINARY_DIR}/verification/overview-product" --verify-overview "{output}")
set_tests_properties(runtime-overview PROPERTIES TIMEOUT 150
    ENVIRONMENT_MODIFICATION "QSG_NO_VSYNC=unset:"
    ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1"
    FAIL_REGULAR_EXPRESSION "Binding loop;Cannot assign;ReferenceError;TypeError;Failed to compile")

yalahoo_ui_test(overview-qml --overview overview-qml 120)

add_test(NAME opening-qml COMMAND ui-tests --opening "${CMAKE_BINARY_DIR}/verification/opening")
set_tests_properties(opening-qml PROPERTIES TIMEOUT 900 ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QSG_RENDER_LOOP=threaded;QCPAINTER_DEBUG_COLLECT=0")

yalahoo_ui_test(library-panels-qml --library library-panels 240 THREADED)

yalahoo_ui_test(playback-qml --playback playback-qml 240 THREADED)

yalahoo_ui_test(settings-qml --settings settings-qml 240 THREADED ARGUMENTS --representative)

yalahoo_ui_test(experiments-qml --experiments experiments-qml 240 THREADED)

yalahoo_ui_test(controls-qml --controls controls-qml 60 THREADED)

add_test(NAME startup-qml COMMAND ui-tests --startup "${CMAKE_BINARY_DIR}/verification/startup")
set_tests_properties(startup-qml PROPERTIES TIMEOUT 900
    ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QSG_RENDER_LOOP=threaded"
    FAIL_REGULAR_EXPRESSION "Binding loop;Cannot assign;ReferenceError;TypeError;Failed to compile")

if(APPLE)
    qt_add_executable(MacApplicationMenuTest MacApplicationMenuTest.mm
        "${PROJECT_SOURCE_DIR}/src/platform/macos/ApplicationMenu.mm")
    target_include_directories(MacApplicationMenuTest PRIVATE "${PROJECT_SOURCE_DIR}/src/app"
        "${PROJECT_SOURCE_DIR}/src/platform/macos")
    target_link_libraries(MacApplicationMenuTest PRIVATE YalahooVerification Qt6::Quick Qt6::Test "-framework AppKit")
    yalahoo_platform(MacApplicationMenuTest)
    add_test(NAME macos-application-menu COMMAND MacApplicationMenuTest)
    set_tests_properties(macos-application-menu PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
endif()
