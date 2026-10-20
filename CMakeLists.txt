cmake_minimum_required(VERSION 3.25)
project(yalahoo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_definitions(QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII QT_NO_KEYWORDS)
add_custom_target(source-policy ALL
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${PROJECT_SOURCE_DIR}/tools/audit/CheckSourcePolicy.cmake"
    COMMENT "Check ASCII sources and explicit Qt string policy"
    VERBATIM)
set(CMAKE_TRY_COMPILE_CONFIGURATION "${CMAKE_BUILD_TYPE}")
if(MSVC)
    add_compile_options(/Zc:__cplusplus /utf-8)
endif()
include(cmake/Cxx23.cmake)
include(cmake/QtKit.cmake)
include(cmake/Platform.cmake)
yalahoo_discover_qt()
find_package(Qt6 REQUIRED COMPONENTS ${YALAHOO_QT_COMPONENTS})
find_package(Qt6 REQUIRED COMPONENTS QuickPrivate)
if(APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 23)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
    set(CMAKE_OBJCXX_EXTENSIONS OFF)
endif()
include(cmake/Optimization.cmake)
if(APPLE)
    add_library(YalahooVerification STATIC src/app/VerificationWindowMac.mm)
    target_link_libraries(YalahooVerification PUBLIC Qt6::Gui PRIVATE "-framework AppKit" "-framework CoreGraphics")
endif()
if(NOT Qt6_VERSION STREQUAL YALAHOO_QT_VERSION)
    message(FATAL_ERROR "Resolved Qt version differs from discovered Kit")
endif()
yalahoo_check_tool(qmake "-query;QT_VERSION")
yalahoo_check_tool(qsb "--version")
yalahoo_check_tool(qmllint "--version")
yalahoo_check_tool(lupdate "-version")
yalahoo_check_tool(lrelease "-version")
qt_standard_project_setup(I18N_SOURCE_LANGUAGE en I18N_TRANSLATED_LANGUAGES zh_CN)
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)
option(YALAHOO_REVIEW "Enable deterministic geometry review in the application" OFF)
option(YALAHOO_PERFORMANCE_RECORD_ONLY "Record Review performance without enforcing numeric targets" OFF)
option(YALAHOO_MACOS_VISIBLE_TESTS "Enable deferred macOS native window verification" OFF)
if(YALAHOO_REVIEW)
    find_package(Qt6 REQUIRED COMPONENTS Quick3DPrivate Quick3DRuntimeRenderPrivate)
endif()
include(cmake/RhiProbe.cmake)

add_subdirectory(src/core)
add_subdirectory(src/motion)
add_subdirectory(src/services)
add_subdirectory(src/library)
add_subdirectory(src/media)
add_subdirectory(src/presentation)
add_subdirectory(src/render)
add_subdirectory(tools/assets)
add_subdirectory(tools/review)
add_subdirectory(src/app)
set(translation_source_targets
    Yalahoo YalahooCore YalahooScene YalahooRhi YalahooNavigation YalahooText YalahooOpening ui-audit)
foreach(target IN LISTS translation_source_targets)
    set_property(TARGET "${target}" APPEND PROPERTY QT_EXCLUDE_SOURCES_FROM_TRANSLATION
        "${PROJECT_SOURCE_DIR}/resources/*")
endforeach()
qt_add_translations(
    TARGETS YalahooCore
    SOURCE_TARGETS ${translation_source_targets}
    TS_FILES "${PROJECT_SOURCE_DIR}/resources/i18n/yalahoo_zh_CN.ts"
    PLURALS_TS_FILE "${PROJECT_SOURCE_DIR}/resources/i18n/yalahoo_en.ts"
    QM_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/core"
    RESOURCE_PREFIX /i18n
    LUPDATE_OPTIONS -no-obsolete -locations none
    LRELEASE_OPTIONS -fail-on-unfinished
    MERGE_QT_TRANSLATIONS
    QT_TRANSLATION_CATALOGS qtbase qtdeclarative qtmultimedia
)
enable_testing()
include(cmake/SourceFormat.cmake)
add_subdirectory(tests/unit)
add_subdirectory(tests/integration)
include(cmake/TestPolicy.cmake)

file(
    WRITE
    "${CMAKE_BINARY_DIR}/toolchain.json"
    "{\n  \"qtVersion\": \"${Qt6_VERSION}\",\n  \"qtKit\": \"${YALAHOO_SELECTED_QT}\",\n  \"compiler\": \"${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\",\n  \"cxxStandard\": 23,\n  \"configuration\": \"${CMAKE_BUILD_TYPE}\"\n}\n"
)
