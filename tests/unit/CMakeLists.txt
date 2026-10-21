qt_add_executable(IdentityTest IdentityTest.cpp)
qt_add_executable(DisplayOutputTest DisplayOutputTest.cpp)
target_include_directories(DisplayOutputTest PRIVATE "${PROJECT_SOURCE_DIR}/src/app")
target_link_libraries(DisplayOutputTest PRIVATE YalahooRhi Qt6::Test Qt6::GuiPrivate Qt6::QuickVectorImage)
target_compile_definitions(DisplayOutputTest PRIVATE DISPLAY_OUTPUT_REPORT="${CMAKE_BINARY_DIR}/verification/display-output.json")
set_source_files_properties("${PROJECT_SOURCE_DIR}/qml/Yalahoo/Music/components/DisplaySurface.qml"
    PROPERTIES QT_RESOURCE_ALIAS "components/DisplaySurface.qml")
qt_add_qml_module(DisplayOutputTest URI Yalahoo.DisplayTest
    DEPENDENCIES QtQuick
    QML_FILES DisplayOutputProbe.qml
        "${PROJECT_SOURCE_DIR}/qml/Yalahoo/Music/components/DisplaySurface.qml"
    SOURCES DisplayOutputTypes.h)
qt_add_resources(DisplayOutputTest display_brand PREFIX /display
    BASE "${PROJECT_SOURCE_DIR}/resources/branding"
    FILES "${PROJECT_SOURCE_DIR}/resources/branding/logo-light.svg")
qt_add_shaders(DisplayOutputTest display_transfer PREFIX /assets/shaders
    BASE "${PROJECT_SOURCE_DIR}/resources/shaders"
    FILES "${PROJECT_SOURCE_DIR}/resources/shaders/display-output.frag")
qt_add_shaders(DisplayOutputTest display_vertex PREFIX /display
    BASE "${PROJECT_SOURCE_DIR}/resources/shaders/rhi"
    FILES "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/output.vert"
        "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/output.frag")
yalahoo_platform(DisplayOutputTest)
add_test(NAME display-transfer-gpu COMMAND DisplayOutputTest transferShader)
add_test(NAME display-output-visible COMMAND DisplayOutputTest nativeOutput)
set_tests_properties(display-transfer-gpu display-output-visible PROPERTIES TIMEOUT 30)
set_tests_properties(display-output-visible PROPERTIES ENVIRONMENT "YALAHOO_VERIFY_VISIBLE=1")
yalahoo_unit_test(LocalizationTest localization
    LIBRARIES YalahooCore Qt6::Qml
    TIMEOUT 30)
qt_add_executable(TitleTest TitleTest.cpp "${PROJECT_SOURCE_DIR}/src/app/AssetCatalog.cpp")
target_include_directories(TitleTest PRIVATE "${PROJECT_SOURCE_DIR}/src/app")
target_link_libraries(TitleTest PRIVATE YalahooText Qt6::Test)
include("${PROJECT_SOURCE_DIR}/cmake/Assets.cmake")
yalahoo_add_assets(TitleTest)
yalahoo_platform(TitleTest)
add_test(NAME title-reels COMMAND TitleTest)
set_tests_properties(title-reels PROPERTIES TIMEOUT 60)
qt_add_executable(NavigationTest NavigationTest.cpp)
target_include_directories(NavigationTest PRIVATE "${PROJECT_SOURCE_DIR}/src/app")
target_link_libraries(NavigationTest PRIVATE YalahooNavigation Qt6::Test)
yalahoo_platform(NavigationTest)
add_test(NAME navigation COMMAND NavigationTest)
set_tests_properties(navigation PROPERTIES TIMEOUT 60)
yalahoo_unit_test(CoverCacheTest cover-cache
    LIBRARIES YalahooRhi
    DEFINITIONS COVER_REPORT="${CMAKE_BINARY_DIR}/verification/cover-cache.json"
    TIMEOUT 90)
yalahoo_unit_test(AtlasUploadTest rhi-atlas-queue
    LIBRARIES YalahooRhi)
qt_add_executable(VerificationWindowTest VerificationWindowTest.cpp)
target_include_directories(VerificationWindowTest PRIVATE "${PROJECT_SOURCE_DIR}/src/app")
target_link_libraries(VerificationWindowTest PRIVATE YalahooCore Qt6::Quick Qt6::Test)
target_compile_definitions(VerificationWindowTest PRIVATE
    VISIBLE_WINDOW_REPORT="${CMAKE_BINARY_DIR}/verification/native-window-visible.json")
yalahoo_platform(VerificationWindowTest)
add_test(NAME verification-window-policy COMMAND VerificationWindowTest foregroundOwnership hiddenNativeWindow)
set_tests_properties(verification-window-policy PROPERTIES ENVIRONMENT "YALAHOO_VERIFY_FOREGROUND=0")
if(NOT APPLE OR YALAHOO_MACOS_VISIBLE_TESTS)
    add_test(NAME verification-window-visible COMMAND VerificationWindowTest visibleNativeWindow)
    set_tests_properties(verification-window-visible PROPERTIES TIMEOUT 60
        ENVIRONMENT "YALAHOO_VERIFY_VISIBLE=1;QSG_RENDER_LOOP=threaded"
        ENVIRONMENT_MODIFICATION "QSG_NO_VSYNC=unset:")
endif()
if(WIN32)
    target_link_libraries(VerificationWindowTest PRIVATE user32)
endif()
yalahoo_unit_test(RhiMaterialTest rhi-material-parameters
    LIBRARIES YalahooRhi  Qt6::GuiPrivate
    DEFINITIONS FIXTURE_ROOT="${PROJECT_SOURCE_DIR}")
if(YALAHOO_REVIEW)
    add_test(NAME real-columns-gpu COMMAND archive-pool-audit
        "${CMAKE_BINARY_DIR}/verification/real-columns" --real-columns)
    set_tests_properties(real-columns-gpu PROPERTIES TIMEOUT 240
        ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QT_ENABLE_HIGHDPI_SCALING=0;QSG_RENDER_LOOP=threaded"
        FAIL_REGULAR_EXPRESSION "Failed to compile;QSpirvCompiler;Nonfinite RHI pixel")
    add_test(NAME archive-pool-gpu COMMAND archive-pool-audit
        "${CMAKE_BINARY_DIR}/verification/archive-pool")
    set_tests_properties(archive-pool-gpu PROPERTIES TIMEOUT 240
        ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QT_ENABLE_HIGHDPI_SCALING=0;QSG_RENDER_LOOP=threaded"
        FAIL_REGULAR_EXPRESSION "Failed to compile;QSpirvCompiler;Nonfinite RHI pixel")
    add_test(NAME cover-gpu COMMAND cover-audit "${CMAKE_BINARY_DIR}/verification/covers")
    set_tests_properties(cover-gpu PROPERTIES TIMEOUT 180
        ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QT_ENABLE_HIGHDPI_SCALING=0;QSG_RENDER_LOOP=threaded")
    add_test(NAME rhi-postprocess-gpu COMMAND rhi-postprocess-audit
        "${CMAKE_BINARY_DIR}/verification/rhi-postprocess")
    set_tests_properties(rhi-postprocess-gpu PROPERTIES TIMEOUT 180
        ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QSG_RENDER_LOOP=threaded"
        FAIL_REGULAR_EXPRESSION "Failed to compile;QSpirvCompiler;Nonfinite RHI pixel")
    add_test(NAME rhi-bokeh-gpu COMMAND rhi-bokeh-audit "${CMAKE_BINARY_DIR}/verification/rhi-bokeh")
    set_tests_properties(rhi-bokeh-gpu PROPERTIES TIMEOUT 120
        ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins"
        FAIL_REGULAR_EXPRESSION "Nonfinite;Failed to create;Shader baking failed")
    add_test(NAME rhi-smaa-gpu COMMAND rhi-smaa-audit "${CMAKE_BINARY_DIR}/verification/rhi-smaa")
    set_tests_properties(rhi-smaa-gpu PROPERTIES TIMEOUT 120
        ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins"
        FAIL_REGULAR_EXPRESSION "Nonfinite;Failed to create;Shader baking failed")
    add_test(NAME rhi-ao-gpu COMMAND rhi-ao-audit "${CMAKE_BINARY_DIR}/verification/rhi-ao")
    set_tests_properties(rhi-ao-gpu PROPERTIES TIMEOUT 120
        ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1"
        FAIL_REGULAR_EXPRESSION "Nonfinite;Failed to create;Shader baking failed")
    add_test(NAME rhi-fog-gpu COMMAND rhi-fog-audit "${CMAKE_BINARY_DIR}/verification/rhi-fog")
    set_tests_properties(rhi-fog-gpu PROPERTIES TIMEOUT 120
        ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1"
        FAIL_REGULAR_EXPRESSION "Nonfinite;Failed to create;Shader baking failed")
    add_test(NAME rhi-shadow-gpu COMMAND rhi-shadow-audit "${CMAKE_BINARY_DIR}/verification/rhi-shadow")
    set_tests_properties(rhi-shadow-gpu PROPERTIES TIMEOUT 120
        ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1"
        FAIL_REGULAR_EXPRESSION "Nonfinite;Failed to create;Shader baking failed")
    add_test(NAME rhi-flicker-gpu COMMAND rhi-flicker-audit
        "${CMAKE_BINARY_DIR}/verification/rhi-flicker")
    set_tests_properties(rhi-flicker-gpu PROPERTIES TIMEOUT 420
        ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QT_ENABLE_HIGHDPI_SCALING=1"
        FAIL_REGULAR_EXPRESSION "Binding loop;Failed to create;Shader baking failed;Nonfinite")
    add_test(NAME rhi-scale-gpu COMMAND rhi-scale-audit
        "${CMAKE_BINARY_DIR}/verification/rhi-scale" --representative)
    set_tests_properties(rhi-scale-gpu PROPERTIES TIMEOUT 600
        ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QT_ENABLE_HIGHDPI_SCALING=1"
        FAIL_REGULAR_EXPRESSION "Binding loop;Failed to create;Shader baking failed;Nonfinite")
    add_test(NAME rhi-shell-gpu COMMAND rhi-shell-audit
        "${PROJECT_SOURCE_DIR}/tests/fixtures/rhi-shell/visual.json"
        "${CMAKE_BINARY_DIR}/verification/rhi-shell")
    set_tests_properties(rhi-shell-gpu PROPERTIES TIMEOUT 240
        ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QSG_RENDER_LOOP=threaded"
        FAIL_REGULAR_EXPRESSION "Failed to compile;QSpirvCompiler;Nonfinite RHI pixel")
    add_test(NAME rhi-opaque-gpu COMMAND rhi-audit "${CMAKE_BINARY_DIR}/verification/rhi")
    set_tests_properties(
        rhi-opaque-gpu
        PROPERTIES
        TIMEOUT
        180
        ENVIRONMENT
        "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QT_ENABLE_HIGHDPI_SCALING=0;QSG_RENDER_LOOP=threaded"
    )
endif()
qt_add_executable(RenderQualityTest RenderQualityTest.cpp)
target_link_libraries(RenderQualityTest PRIVATE YalahooScene Qt6::Test)
target_compile_definitions(RenderQualityTest PRIVATE FIXTURE_ROOT="${PROJECT_SOURCE_DIR}")
yalahoo_platform(RenderQualityTest)
add_test(
    NAME
    render-quality
    COMMAND
    RenderQualityTest
    -o
    "${CMAKE_BINARY_DIR}/verification/render-quality-test.xml,junitxml"
    -o
    -,txt
)
qt_add_executable(SceneTest SceneTest.cpp)
target_link_libraries(SceneTest PRIVATE YalahooScene Qt6::Test)
target_compile_definitions(
    SceneTest
    PRIVATE
    FIXTURE_ROOT="${PROJECT_SOURCE_DIR}"
    SCENE_REPORT="${CMAKE_BINARY_DIR}/verification/scene-errors.json"
)
yalahoo_platform(SceneTest)
add_test(
    NAME
    scene
    COMMAND
    SceneTest
    -o
    "${CMAKE_BINARY_DIR}/verification/scene-test.xml,junitxml"
    -o
    -,txt
)
set_tests_properties(scene PROPERTIES TIMEOUT 60)
if(YALAHOO_REVIEW)
    add_test(
        NAME
        runtime-render-backend
        COMMAND
        Yalahoo
        --verify-materials
        "${PROJECT_SOURCE_DIR}/tests/fixtures/render-backend/visual.json"
        --material-report
        "${CMAKE_BINARY_DIR}/verification/render-backend/runtime.json"
    )
    set_tests_properties(
        runtime-render-backend
        PROPERTIES
        TIMEOUT
        240
        FAIL_REGULAR_EXPRESSION
        "Failed to compile;QSpirvCompiler;TypeError;out of range"
        ENVIRONMENT
        "QT_ENABLE_HIGHDPI_SCALING=0;QT_SCALE_FACTOR=1;QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins"
    )
    foreach(dpr IN ITEMS 1 1.5 2)
        add_test(
            NAME
            runtime-postprocess-dpr-${dpr}
            COMMAND
            Yalahoo
            --verify-materials
            "${PROJECT_SOURCE_DIR}/tests/fixtures/postprocess/visual-dpr-${dpr}.json"
            --material-report
            "${CMAKE_BINARY_DIR}/verification/postprocess/dpr-${dpr}/runtime.json"
        )
        set_tests_properties(
            runtime-postprocess-dpr-${dpr}
            PROPERTIES
            TIMEOUT
            420
            FAIL_REGULAR_EXPRESSION
            "Failed to compile;QSpirvCompiler;TypeError;out of range"
            ENVIRONMENT
            "QT_ENABLE_HIGHDPI_SCALING=0;QT_SCALE_FACTOR=${dpr};QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins"
        )
    endforeach()
    add_test(
        NAME
        runtime-occlusion
        COMMAND
        Yalahoo
        --verify-materials
        "${PROJECT_SOURCE_DIR}/tests/fixtures/occlusion/visual.json"
        --material-report
        "${CMAKE_BINARY_DIR}/verification/occlusion/runtime.json"
    )
    set_tests_properties(
        runtime-occlusion
        PROPERTIES
        TIMEOUT
        180
        FAIL_REGULAR_EXPRESSION
        "Failed to compile;QSpirvCompiler;TypeError;out of range"
        ENVIRONMENT
        "QT_ENABLE_HIGHDPI_SCALING=0;QT_SCALE_FACTOR=1;QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins"
    )
    add_test(
        NAME
        runtime-materials
        COMMAND
        Yalahoo
        --verify-materials
        "${PROJECT_SOURCE_DIR}/tests/fixtures/materials/single-case.json"
        --material-report
        "${CMAKE_BINARY_DIR}/verification/materials/runtime.json"
    )
    set_tests_properties(
        runtime-materials
        PROPERTIES
        TIMEOUT
        150
        ENVIRONMENT
        "QT_ENABLE_HIGHDPI_SCALING=0;QT_SCALE_FACTOR=1;QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins"
    )
    add_test(
        NAME
        runtime-lighting
        COMMAND
        Yalahoo
        --verify-materials
        "${PROJECT_SOURCE_DIR}/tests/fixtures/lighting/visual.json"
        --material-report
        "${CMAKE_BINARY_DIR}/verification/lighting/runtime.json"
    )
    set_tests_properties(
        runtime-lighting
        PROPERTIES
        TIMEOUT
        150
        ENVIRONMENT
        "QT_ENABLE_HIGHDPI_SCALING=0;QT_SCALE_FACTOR=1;QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins"
    )
    foreach(dpr IN ITEMS 1 1.5 2)
        add_test(
            NAME
            runtime-scene-dpr-${dpr}
            COMMAND
            Yalahoo
            --verify-scene
            "${PROJECT_SOURCE_DIR}/tests/fixtures/scene/camera-golden.json"
            --scene-report
            "${CMAKE_BINARY_DIR}/verification/scene-dpr-${dpr}/runtime.json"
        )
        set_tests_properties(
            runtime-scene-dpr-${dpr}
            PROPERTIES
            TIMEOUT
            90
            ENVIRONMENT
            "QT_ENABLE_HIGHDPI_SCALING=0;QT_SCALE_FACTOR=${dpr};QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QSG_INFO=1"
        )
    endforeach()
endif()
target_link_libraries(IdentityTest PRIVATE YalahooCore Qt6::Gui Qt6::Test)
yalahoo_platform(IdentityTest)
add_test(NAME identity COMMAND IdentityTest)
qt_add_executable(ModuleProbe ModuleProbe.cpp)
target_link_libraries(
    ModuleProbe
    PRIVATE
    Qt6::Gui
    Qt6::Test
    Qt6::Quick3D
    Qt6::Multimedia
    Qt6::ShaderTools
)
yalahoo_platform(ModuleProbe)
add_test(NAME module-link-and-plugins COMMAND ModuleProbe)
add_test(NAME production-app-root COMMAND Yalahoo --verify-app)
set_tests_properties(production-app-root PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE
    FAIL_REGULAR_EXPRESSION "Binding loop;Cannot assign;ReferenceError;TypeError;Failed to compile")
add_test(
    NAME
    shader-tools
    COMMAND
    "${YALAHOO_qsb}"
    --dump
    "${CMAKE_BINARY_DIR}/src/app/.qsb/probe.frag.qsb"
)
set_tests_properties(shader-tools PROPERTIES PASS_REGULAR_EXPRESSION "MSL.*Standard")
add_test(
    NAME
    kit-selection
    COMMAND
    "${CMAKE_COMMAND}"
    -P
    "${CMAKE_CURRENT_SOURCE_DIR}/KitSelectionTest.cmake"
)
add_test(
    NAME
    runtime
    COMMAND
    Yalahoo
    --verify-runtime
    "${CMAKE_BINARY_DIR}/verification/runtime.json"
)
set_tests_properties(runtime PROPERTIES TIMEOUT 30)
qt_add_executable(MotionTest MotionTest.cpp)
target_link_libraries(MotionTest PRIVATE YalahooCore Qt6::Test)
target_compile_definitions(
    MotionTest
    PRIVATE
    FIXTURE_ROOT="${PROJECT_SOURCE_DIR}"
    MOTION_REPORT="${CMAKE_BINARY_DIR}/verification/motion-errors.json"
)
yalahoo_platform(MotionTest)
add_test(
    NAME
    motion
    COMMAND
    MotionTest
    -o
    "${CMAKE_BINARY_DIR}/verification/motion-test.xml,junitxml"
    -o
    -,txt
)
set_tests_properties(motion PROPERTIES TIMEOUT 60)
add_test(
    NAME
    runtime-motion
    COMMAND
    Yalahoo
    --verify-runtime
    "${CMAKE_BINARY_DIR}/verification/runtime-motion.json"
    --verify-motion
)
set_tests_properties(
    runtime-motion
    PROPERTIES
    TIMEOUT
    30
    ENVIRONMENT
    "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QSG_INFO=1"
)
set_tests_properties(
    identity
    module-link-and-plugins
    shader-tools
    runtime
    PROPERTIES
    ENVIRONMENT
    "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QSG_INFO=1"
)
qt_add_executable(LibraryDataTest LibraryDataTest.cpp)
yalahoo_unit_test(DirectoryScannerTest directory-scanner
    LIBRARIES YalahooCore
    TIMEOUT 90)
target_link_libraries(LibraryDataTest PRIVATE YalahooCore Qt6::Gui Qt6::Test)
target_compile_definitions(LibraryDataTest PRIVATE FIXTURE_ROOT="${PROJECT_SOURCE_DIR}")
yalahoo_platform(LibraryDataTest)
add_test(
    NAME
    library-data
    COMMAND
    LibraryDataTest
    -o
    "${CMAKE_BINARY_DIR}/verification/library-data-test.xml,junitxml"
    -o
    -,txt
)
set_tests_properties(
    library-data
    PROPERTIES
    TIMEOUT
    60
    ENVIRONMENT
    "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins"
)
add_test(
    NAME
    runtime-library
    COMMAND
    Yalahoo
    --verify-runtime
    "${CMAKE_BINARY_DIR}/verification/runtime-library.json"
    --verify-library
    "${PROJECT_SOURCE_DIR}/tests/fixtures/web-baseline/anonymous.json"
)
set_tests_properties(
    runtime-library
    PROPERTIES
    TIMEOUT
    30
    ENVIRONMENT
    "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QSG_INFO=1"
)

qt_add_executable(LightingTest LightingTest.cpp)
target_link_libraries(LightingTest PRIVATE YalahooScene Qt6::Test)
target_compile_definitions(
    LightingTest
    PRIVATE
    FIXTURE_ROOT="${PROJECT_SOURCE_DIR}"
    LIGHTING_REPORT="${CMAKE_BINARY_DIR}/verification/lighting-errors.json"
)
yalahoo_platform(LightingTest)
add_test(
    NAME
    lighting
    COMMAND
    LightingTest
    -o
    "${CMAKE_BINARY_DIR}/verification/lighting-test.xml,junitxml"
    -o
    -,txt
)
set_tests_properties(lighting PROPERTIES TIMEOUT 60)

if(YALAHOO_REVIEW)
    add_test(NAME runtime-rhi-integration COMMAND Yalahoo --verify-integration
        "${CMAKE_BINARY_DIR}/verification/rhi-integration/ctest")
    set_tests_properties(runtime-rhi-integration PROPERTIES TIMEOUT 90
        ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QT_ENABLE_HIGHDPI_SCALING=0;QSG_RENDER_LOOP=threaded"
        ENVIRONMENT_MODIFICATION "QSG_NO_VSYNC=unset:"
        FAIL_REGULAR_EXPRESSION "Failed to compile;QSpirvCompiler;Nonfinite RHI pixel")
endif()
qt_add_executable(ArchivePoolTest ArchivePoolTest.cpp)
yalahoo_unit_test(RealColumnsTest real-columns
    LIBRARIES YalahooScene
    DEFINITIONS FIXTURE_ROOT="${PROJECT_SOURCE_DIR}"
    TIMEOUT 60)
target_link_libraries(ArchivePoolTest PRIVATE YalahooScene Qt6::Test)
target_compile_definitions(ArchivePoolTest PRIVATE FIXTURE_ROOT="${PROJECT_SOURCE_DIR}")
yalahoo_platform(ArchivePoolTest)
add_test(NAME archive-pool COMMAND ArchivePoolTest)
set_tests_properties(archive-pool PROPERTIES TIMEOUT 60)
add_test(NAME runtime-filled COMMAND Yalahoo --verify-runtime
    "${CMAKE_BINARY_DIR}/verification/runtime-filled/runtime.json" --verify-shading --verify-filled)
set_tests_properties(runtime-filled PROPERTIES TIMEOUT 60
    ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QT_ENABLE_HIGHDPI_SCALING=0")

yalahoo_unit_test(PresentationTest presentation
    LIBRARIES YalahooCore
    TIMEOUT 60)
yalahoo_unit_test(DetailMotionTest detail-motion
    LIBRARIES YalahooScene
    DEFINITIONS FIXTURE_ROOT="${PROJECT_SOURCE_DIR}"
    TIMEOUT 60)
yalahoo_unit_test(OverviewTest overview
    LIBRARIES YalahooScene
    DEFINITIONS FIXTURE_ROOT="${PROJECT_SOURCE_DIR}"
    TIMEOUT 60)
yalahoo_unit_test(OpeningTest opening
    LIBRARIES YalahooOpening
    DEFINITIONS FIXTURE_ROOT="${PROJECT_SOURCE_DIR}")
yalahoo_unit_test(MediaMetadataTest media-metadata
    LIBRARIES YalahooCore
    TIMEOUT 90 SERIAL)
yalahoo_unit_test(TrackFocusTest track-focus
    LIBRARIES YalahooScene
    TIMEOUT 60)
qt_add_executable(PlaybackTest PlaybackTest.cpp)
target_include_directories(PlaybackTest PRIVATE "${PROJECT_SOURCE_DIR}/tests/support")
target_link_libraries(PlaybackTest PRIVATE YalahooCore Qt6::Test)
yalahoo_platform(PlaybackTest)
add_test(NAME playback COMMAND PlaybackTest)
set_tests_properties(playback PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)

qt_add_executable(AudioAmbienceTest AudioAmbienceTest.cpp)
target_include_directories(AudioAmbienceTest PRIVATE "${PROJECT_SOURCE_DIR}/tests/support")
target_link_libraries(AudioAmbienceTest PRIVATE YalahooCore Qt6::Test)
yalahoo_platform(AudioAmbienceTest)
include("${PROJECT_SOURCE_DIR}/cmake/Assets.cmake")
yalahoo_add_assets(AudioAmbienceTest)
add_test(NAME audio-ambience COMMAND AudioAmbienceTest)
set_tests_properties(audio-ambience PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)

yalahoo_unit_test(SettingsTest settings
    LIBRARIES YalahooScene
    TIMEOUT 60)
qt_add_executable(ExperimentTest ExperimentTest.cpp)
target_include_directories(ExperimentTest PRIVATE "${PROJECT_SOURCE_DIR}/src/render/rhi")
target_link_libraries(ExperimentTest PRIVATE YalahooScene Qt6::Test)
yalahoo_platform(ExperimentTest)
add_test(NAME experiments COMMAND ExperimentTest)
set_tests_properties(experiments PROPERTIES TIMEOUT 60)

yalahoo_unit_test(StartupTest startup
    LIBRARIES YalahooScene
    TIMEOUT 60)
yalahoo_unit_test(LoggingTest logging
    LIBRARIES YalahooCore)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME dry-tools COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tests/tools/DryToolsTest.py")
set_tests_properties(dry-tools PROPERTIES TIMEOUT 10)
add_test(NAME startup-verification COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tests/tools/StartupVerificationTest.py")
set_tests_properties(startup-verification PROPERTIES TIMEOUT 10)
add_test(NAME logging-terminal COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tests/tools/LoggingTerminalTest.py" "$<TARGET_FILE:LoggingTest>")
set_tests_properties(logging logging-terminal PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)

qt_add_executable(GraphicsBackendTest GraphicsBackendTest.cpp)
target_include_directories(GraphicsBackendTest PRIVATE "${PROJECT_SOURCE_DIR}/src/app")
target_link_libraries(GraphicsBackendTest PRIVATE YalahooRhi Qt6::Test)
yalahoo_platform(GraphicsBackendTest)
add_test(NAME graphics-backend COMMAND GraphicsBackendTest)
set_tests_properties(graphics-backend PROPERTIES TIMEOUT 10)

yalahoo_unit_test(OnlineMetadataTest online-metadata
    LIBRARIES YalahooCore  Qt6::Network
    TIMEOUT 60)
