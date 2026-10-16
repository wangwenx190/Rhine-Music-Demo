qt_add_executable(
    AssetTest
    AssetTest.cpp
    "${PROJECT_SOURCE_DIR}/tools/assets/GeometryAudit.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/AssetCatalog.cpp"
)
target_include_directories(
    AssetTest
    PRIVATE
    "${PROJECT_SOURCE_DIR}/tools/assets"
    "${PROJECT_SOURCE_DIR}/src/app"
)
target_link_libraries(
    AssetTest
    PRIVATE
    Qt6::Gui
    Qt6::Test
    Qt6::Quick3D
    Qt6::Qml
    Qt6::Svg
    Qt6::Multimedia
)
include("${PROJECT_SOURCE_DIR}/cmake/Assets.cmake")
yalahoo_add_assets(AssetTest)
yalahoo_platform(AssetTest)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/verification")
add_test(
    NAME
    assets
    COMMAND
    AssetTest
    -o
    "${CMAKE_BINARY_DIR}/verification/assets-test.xml,junitxml"
    -o
    -,txt
)
set_tests_properties(
    assets
    PROPERTIES
    TIMEOUT
    90
    WORKING_DIRECTORY
    "${CMAKE_BINARY_DIR}/verification"
    ENVIRONMENT
    "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins"
)
if(WIN32)
    set_tests_properties(
        assets
        PROPERTIES
        ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin"
    )
endif()
qt_add_executable(LibraryQmlTest LibraryQmlTest.cpp)
add_test(NAME ui-drawing COMMAND ui-audit "${CMAKE_BINARY_DIR}/verification/ui-drawing" --representative)
add_test(NAME ruler-drawing COMMAND ui-audit --ruler "${CMAKE_BINARY_DIR}/verification/ruler-drawing")
set_tests_properties(ruler-drawing PROPERTIES TIMEOUT 120
    ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QSG_RENDER_LOOP=threaded;QT_SCALE_FACTOR=1"
    FAIL_REGULAR_EXPRESSION "Binding loop;Cannot assign;ReferenceError;TypeError;Failed to compile")
if(WIN32)
    set_tests_properties(ruler-drawing PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
endif()
# Match migration verification's render loop and explicit native scale factor.
# Factor 1 keeps the screen's real DPR; it does not force a DPR1 framebuffer.
set_tests_properties(ui-drawing PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QSG_RENDER_LOOP=threaded;QT_SCALE_FACTOR=1")
if(WIN32)
    set_tests_properties(ui-drawing PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
endif()
target_link_libraries(LibraryQmlTest PRIVATE YalahooCore Qt6::Qml Qt6::Quick Qt6::Test)
target_compile_definitions(LibraryQmlTest PRIVATE FIXTURE_ROOT="${PROJECT_SOURCE_DIR}")
yalahoo_platform(LibraryQmlTest)
add_test(
    NAME
    library-qml
    COMMAND
    LibraryQmlTest
    -o
    "${CMAKE_BINARY_DIR}/verification/library-qml-test.xml,junitxml"
    -o
    -,txt
)
set_tests_properties(
    library-qml
    PROPERTIES
    TIMEOUT
    30
    ENVIRONMENT
    "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins"
)
if(WIN32)
    set_tests_properties(
        library-qml
        PROPERTIES
        ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin"
    )
endif()
add_test(
    NAME
    branding
    COMMAND
    export-branding
    "${PROJECT_SOURCE_DIR}"
    --verify-only
    --report
    "${CMAKE_BINARY_DIR}/verification/branding-report.json"
)
set_tests_properties(
    branding
    PROPERTIES
    TIMEOUT
    90
    ENVIRONMENT
    "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins"
)
if(WIN32)
    set_tests_properties(
        branding
        PROPERTIES
        ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin"
    )
endif()

add_test(NAME presentation-qml COMMAND ui-audit --presentation
    "${CMAKE_BINARY_DIR}/verification/presentation")
set_tests_properties(presentation-qml PROPERTIES TIMEOUT 90
    ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1"
    FAIL_REGULAR_EXPRESSION "Binding loop;Cannot assign;ReferenceError;TypeError;Failed to compile")
if(WIN32)
    set_tests_properties(presentation-qml PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
endif()

add_test(NAME runtime-detail COMMAND Yalahoo --verify-detail
    "${CMAKE_BINARY_DIR}/verification/detail-product")
set_tests_properties(runtime-detail PROPERTIES TIMEOUT 150
    ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QSG_NO_VSYNC="
    FAIL_REGULAR_EXPRESSION "Binding loop;Cannot assign;ReferenceError;TypeError;Failed to compile")
if(WIN32)
    set_tests_properties(runtime-detail PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
endif()

add_test(NAME runtime-overview COMMAND Yalahoo --verify-overview
    "${CMAKE_BINARY_DIR}/verification/overview-product")
set_tests_properties(runtime-overview PROPERTIES TIMEOUT 150
    ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QSG_NO_VSYNC="
    FAIL_REGULAR_EXPRESSION "Binding loop;Cannot assign;ReferenceError;TypeError;Failed to compile")
if(WIN32)
    set_tests_properties(runtime-overview PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
endif()

add_test(NAME overview-qml COMMAND ui-audit --overview
    "${CMAKE_BINARY_DIR}/verification/overview-qml")
set_tests_properties(overview-qml PROPERTIES TIMEOUT 120
    ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1"
    FAIL_REGULAR_EXPRESSION "Binding loop;Cannot assign;ReferenceError;TypeError;Failed to compile")
if(WIN32)
    set_tests_properties(overview-qml PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
endif()

add_test(NAME opening-qml COMMAND ui-audit --opening "${CMAKE_BINARY_DIR}/verification/opening")
set_tests_properties(opening-qml PROPERTIES TIMEOUT 900 ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QSG_RENDER_LOOP=threaded;QCPAINTER_DEBUG_COLLECT=0")
if(WIN32)
    set_tests_properties(opening-qml PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
endif()

add_test(NAME library-panels-qml COMMAND ui-audit --library "${CMAKE_BINARY_DIR}/verification/library-panels")
set_tests_properties(library-panels-qml PROPERTIES TIMEOUT 240
    ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QSG_RENDER_LOOP=threaded;QT_SCALE_FACTOR=1"
    FAIL_REGULAR_EXPRESSION "Binding loop;Cannot assign;ReferenceError;TypeError;Failed to compile")
if(WIN32)
    set_tests_properties(library-panels-qml PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
endif()

add_test(NAME playback-qml COMMAND ui-audit --playback "${CMAKE_BINARY_DIR}/verification/playback-qml")
set_tests_properties(playback-qml PROPERTIES TIMEOUT 240
    ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QSG_RENDER_LOOP=threaded;QT_SCALE_FACTOR=1"
    FAIL_REGULAR_EXPRESSION "Binding loop;Cannot assign;ReferenceError;TypeError;Failed to compile")
if(WIN32)
    set_tests_properties(playback-qml PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
endif()

add_test(NAME settings-qml COMMAND ui-audit --settings "${CMAKE_BINARY_DIR}/verification/settings-qml")
set_tests_properties(settings-qml PROPERTIES TIMEOUT 240
    ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QSG_RENDER_LOOP=threaded"
    FAIL_REGULAR_EXPRESSION "Binding loop;Cannot assign;ReferenceError;TypeError;Failed to compile")
if(WIN32)
    set_tests_properties(settings-qml PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
endif()

add_test(NAME experiments-qml COMMAND ui-audit --experiments "${CMAKE_BINARY_DIR}/verification/experiments-qml")
set_tests_properties(experiments-qml PROPERTIES TIMEOUT 240
    ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QSG_RENDER_LOOP=threaded"
    FAIL_REGULAR_EXPRESSION "Binding loop;Cannot assign;ReferenceError;TypeError;Failed to compile")
if(WIN32)
    set_tests_properties(experiments-qml PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
endif()

add_test(NAME controls-qml COMMAND ui-audit --controls "${CMAKE_BINARY_DIR}/verification/controls-qml")
set_tests_properties(controls-qml PROPERTIES TIMEOUT 60
    ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QSG_RENDER_LOOP=threaded"
    FAIL_REGULAR_EXPRESSION "Binding loop;Cannot assign;ReferenceError;TypeError;Failed to compile")
if(WIN32)
    set_tests_properties(controls-qml PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
endif()
