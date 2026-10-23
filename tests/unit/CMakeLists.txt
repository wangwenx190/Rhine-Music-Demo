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
add_test(NAME display-output-probe COMMAND DisplayOutputTest asynchronousMonitorDoesNotFreezeProvisionalSdr)
set_tests_properties(display-output-probe PROPERTIES TIMEOUT 10 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
add_test(NAME display-transfer-gpu COMMAND DisplayOutputTest transferShader)
add_test(NAME display-output-visible COMMAND DisplayOutputTest nativeOutput)
set_tests_properties(display-transfer-gpu display-output-visible PROPERTIES TIMEOUT 30)
set_tests_properties(display-output-visible PROPERTIES ENVIRONMENT "YALAHOO_VERIFY_VISIBLE=1")
yalahoo_unit_test(LocalizationTest localization
    LIBRARIES YalahooCore Qt6::Qml
    TIMEOUT 30)
yalahoo_unit_test(TitleTest title-reels
    SOURCES "${PROJECT_SOURCE_DIR}/src/app/AssetCatalog.cpp"
    INCLUDES "${PROJECT_SOURCE_DIR}/src/app"
    LIBRARIES YalahooText TIMEOUT 60)
include("${PROJECT_SOURCE_DIR}/cmake/Assets.cmake")
yalahoo_add_assets(TitleTest)
yalahoo_unit_test(NavigationTest navigation
    LIBRARIES YalahooNavigation
    INCLUDES "${PROJECT_SOURCE_DIR}/src/app"
    TIMEOUT 60)
yalahoo_unit_test(CoverCacheTest cover-cache
    LIBRARIES YalahooRhi
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
set_tests_properties(verification-window-policy PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE
    ENVIRONMENT "YALAHOO_VERIFY_FOREGROUND=0")
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
    LIBRARIES YalahooRhi  Qt6::GuiPrivate Qt6::Quick3D
    DEFINITIONS FIXTURE_ROOT="${PROJECT_SOURCE_DIR}")
if(YALAHOO_REVIEW)
    add_test(NAME real-columns-gpu COMMAND archive-pool-test
        "${CMAKE_BINARY_DIR}/verification/real-columns" --real-columns)
    add_test(NAME archive-pool-gpu COMMAND archive-pool-test
        "${CMAKE_BINARY_DIR}/verification/archive-pool")
    add_test(NAME cover-gpu COMMAND cover-test "${CMAKE_BINARY_DIR}/verification/covers")
    add_test(NAME rhi-postprocess-gpu COMMAND rhi-postprocess-test
        "${CMAKE_BINARY_DIR}/verification/rhi-postprocess")
    add_test(NAME rhi-transient-attachments-gpu COMMAND rhi-postprocess-test
        "${CMAKE_BINARY_DIR}/verification/rhi-transient-attachments" --attachments-only)
    add_test(NAME rhi-bokeh-gpu COMMAND rhi-bokeh-test "${CMAKE_BINARY_DIR}/verification/rhi-bokeh")
    add_test(NAME rhi-smaa-gpu COMMAND rhi-smaa-test "${CMAKE_BINARY_DIR}/verification/rhi-smaa")
    add_test(NAME rhi-ao-gpu COMMAND rhi-ao-test "${CMAKE_BINARY_DIR}/verification/rhi-ao")
    add_test(NAME rhi-fog-gpu COMMAND rhi-fog-test "${CMAKE_BINARY_DIR}/verification/rhi-fog")
    add_test(NAME rhi-shadow-gpu COMMAND rhi-shadow-test "${CMAKE_BINARY_DIR}/verification/rhi-shadow")
    add_test(NAME rhi-shadow-attachments-gpu COMMAND rhi-shadow-test
        "${CMAKE_BINARY_DIR}/verification/rhi-shadow-attachments" --attachments-only)
    add_test(NAME rhi-flicker-gpu COMMAND rhi-flicker-test
        "${CMAKE_BINARY_DIR}/verification/rhi-flicker")
    add_test(NAME rhi-scale-gpu COMMAND rhi-scale-test
        "${CMAKE_BINARY_DIR}/verification/rhi-scale" --representative)
    add_test(NAME rhi-shell-gpu COMMAND rhi-shell-test
        "${PROJECT_SOURCE_DIR}/tests/fixtures/rhi-shell/visual.json"
        "${CMAKE_BINARY_DIR}/verification/rhi-shell")
    add_test(NAME rhi-opaque-gpu COMMAND rhi-opaque-test "${CMAKE_BINARY_DIR}/verification/rhi")

    set_tests_properties(real-columns-gpu archive-pool-gpu rhi-shell-gpu PROPERTIES TIMEOUT "240")

    set_tests_properties(real-columns-gpu archive-pool-gpu cover-gpu rhi-opaque-gpu PROPERTIES ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QT_ENABLE_HIGHDPI_SCALING=0;QSG_RENDER_LOOP=threaded")

    set_tests_properties(real-columns-gpu archive-pool-gpu rhi-postprocess-gpu rhi-transient-attachments-gpu rhi-shell-gpu PROPERTIES FAIL_REGULAR_EXPRESSION "Failed to compile;QSpirvCompiler;Nonfinite RHI pixel")

    set_tests_properties(cover-gpu rhi-postprocess-gpu rhi-opaque-gpu PROPERTIES TIMEOUT "180")

    set_tests_properties(rhi-postprocess-gpu rhi-transient-attachments-gpu rhi-shell-gpu PROPERTIES ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QSG_RENDER_LOOP=threaded")

    set_tests_properties(rhi-transient-attachments-gpu rhi-shadow-attachments-gpu PROPERTIES TIMEOUT "60")

    set_tests_properties(rhi-bokeh-gpu rhi-smaa-gpu rhi-ao-gpu rhi-fog-gpu rhi-shadow-gpu PROPERTIES TIMEOUT "120")

    set_tests_properties(rhi-bokeh-gpu rhi-smaa-gpu PROPERTIES ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins")

    set_tests_properties(rhi-bokeh-gpu rhi-smaa-gpu rhi-ao-gpu rhi-fog-gpu rhi-shadow-gpu rhi-shadow-attachments-gpu PROPERTIES FAIL_REGULAR_EXPRESSION "Nonfinite;Failed to create;Shader baking failed")

    set_tests_properties(rhi-ao-gpu rhi-fog-gpu rhi-shadow-gpu rhi-shadow-attachments-gpu PROPERTIES ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1")

    set_tests_properties(rhi-flicker-gpu PROPERTIES TIMEOUT "420")

    set_tests_properties(rhi-flicker-gpu rhi-scale-gpu PROPERTIES ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QT_ENABLE_HIGHDPI_SCALING=1")

    set_tests_properties(rhi-flicker-gpu rhi-scale-gpu PROPERTIES FAIL_REGULAR_EXPRESSION "Binding loop;Failed to create;Shader baking failed;Nonfinite")

    set_tests_properties(rhi-scale-gpu PROPERTIES TIMEOUT "600")
endif()
yalahoo_unit_test(RenderQualityTest render-quality
    LIBRARIES YalahooScene)
yalahoo_unit_test(SceneTest scene
    LIBRARIES YalahooScene
    DEFINITIONS FIXTURE_ROOT="${PROJECT_SOURCE_DIR}"
    TIMEOUT 60)
if(YALAHOO_REVIEW)
    yalahoo_material_test(runtime-materials materials/single-case.json materials/runtime.json 150)
    yalahoo_material_test(runtime-lighting lighting/visual.json lighting/runtime.json 150)
endif()
yalahoo_unit_test(IdentityTest identity LIBRARIES YalahooCore Qt6::Gui)
yalahoo_unit_test(ModuleProbe module-link-and-plugins
    LIBRARIES Qt6::Gui Qt6::Quick3D Qt6::Multimedia Qt6::ShaderTools)
add_test(NAME production-app-root COMMAND Yalahoo --verify-app)
set_tests_properties(production-app-root PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE
    FAIL_REGULAR_EXPRESSION "Binding loop;Cannot assign;ReferenceError;TypeError;Failed to compile")
add_test(NAME shader-tools COMMAND "${CMAKE_COMMAND}"
    "-DQSB=${YALAHOO_qsb}" "-DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
    -P "${PROJECT_SOURCE_DIR}/tests/tools/ShaderVariantsTest.cmake")
set_tests_properties(shader-tools PROPERTIES TIMEOUT 35)
add_test(
    NAME
    kit-selection
    COMMAND
    "${CMAKE_COMMAND}"
    -P
    "${CMAKE_CURRENT_SOURCE_DIR}/KitSelectionTest.cmake"
)
set_tests_properties(kit-selection PROPERTIES TIMEOUT 10)
yalahoo_unit_test(MotionTest motion
    LIBRARIES YalahooCore
    DEFINITIONS FIXTURE_ROOT="${PROJECT_SOURCE_DIR}"
    TIMEOUT 60)
yalahoo_unit_test(DirectoryScannerTest directory-scanner
    LIBRARIES YalahooCore
    TIMEOUT 90)
yalahoo_unit_test(LibraryDataTest library-data
    LIBRARIES YalahooCore Qt6::Gui
    DEFINITIONS FIXTURE_ROOT="${PROJECT_SOURCE_DIR}"
    TIMEOUT 60
    ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins")
yalahoo_unit_test(LightingTest lighting
    LIBRARIES YalahooScene
    DEFINITIONS FIXTURE_ROOT="${PROJECT_SOURCE_DIR}"
    TIMEOUT 60)

yalahoo_unit_test(RealColumnsTest real-columns
    LIBRARIES YalahooScene
    DEFINITIONS FIXTURE_ROOT="${PROJECT_SOURCE_DIR}"
    TIMEOUT 60)
yalahoo_unit_test(ArchivePoolTest archive-pool
    LIBRARIES YalahooScene
    DEFINITIONS FIXTURE_ROOT="${PROJECT_SOURCE_DIR}"
    TIMEOUT 60)
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
yalahoo_unit_test(PlaybackTest playback
    LIBRARIES YalahooCore
    ARGUMENTS queueRejectsCandidatesAtomically queueReuseAndSnapshotInvalidation lateLoadEndAndQueueOwnership
        outputDetachmentDoesNotInvalidateLoadedMedia
    TIMEOUT 30)
add_test(NAME playback-device COMMAND PlaybackTest publicPlaybackPauseSeekStopAndNavigation
    outputLossPendingIntentAndExplicitSelection realEndAndUnsupportedFormat)
set_tests_properties(playback-device PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)

yalahoo_unit_test(AudioAmbienceTest audio-ambience
    LIBRARIES YalahooCore
    ARGUMENTS missingEventsAreSilent audioPreferencesPersistIndependently
        importedAudioAppliesAtomicallyWithoutSavingIntermediateValues
    TIMEOUT 30)
add_test(NAME audio-ambience-device COMMAND AudioAmbienceTest realBgmGateInterruptionsAndHiddenPlayback
    bgmErrorIsSilentAndExplicitEnableRetries soundEffectPoolIsBounded)
set_tests_properties(audio-ambience-device PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)
include("${PROJECT_SOURCE_DIR}/cmake/Assets.cmake")
yalahoo_add_assets(AudioAmbienceTest)

yalahoo_unit_test(SingleInstanceTest single-instance
    LIBRARIES YalahooCore
    TIMEOUT 30)

yalahoo_unit_test(SettingsTest settings
    LIBRARIES YalahooScene
    TIMEOUT 60)
yalahoo_unit_test(ExperimentTest experiments
    LIBRARIES YalahooScene
    INCLUDES "${PROJECT_SOURCE_DIR}/src/render/rhi"
    TIMEOUT 60)

yalahoo_unit_test(StartupTest startup
    SOURCES "${PROJECT_SOURCE_DIR}/src/app/LibraryPersistence.cpp"
    INCLUDES "${PROJECT_SOURCE_DIR}/src/app"
    LIBRARIES YalahooScene
    TIMEOUT 60)
yalahoo_unit_test(LoggingTest logging
    LIBRARIES YalahooCore)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
if(YALAHOO_REVIEW)
    add_test(NAME rhi-required-output COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tests/tools/RequiredImageOutputTest.py" "$<TARGET_FILE:rhi-flicker-test>")
    set_tests_properties(rhi-required-output PROPERTIES TIMEOUT 40)
endif()
add_test(NAME dry-tools COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tests/tools/DryToolsTest.py")
set_tests_properties(dry-tools PROPERTIES TIMEOUT 10)
add_test(NAME startup-verification COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tests/tools/StartupVerificationTest.py")
set_tests_properties(startup-verification PROPERTIES TIMEOUT 10)
add_test(NAME structure-comparison COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tests/tools/StructureComparisonTest.py")
add_test(NAME test-policy COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tests/tools/TestPolicyTest.py")
set_tests_properties(structure-comparison test-policy PROPERTIES TIMEOUT 30)
add_test(NAME logging-terminal COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tests/tools/LoggingTerminalTest.py" "$<TARGET_FILE:LoggingTest>")
set_tests_properties(logging logging-terminal PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)

yalahoo_unit_test(GraphicsBackendTest graphics-backend
    LIBRARIES YalahooRhi
    INCLUDES "${PROJECT_SOURCE_DIR}/src/app"
    TIMEOUT 10)

yalahoo_unit_test(OnlineMetadataTest online-metadata
    LIBRARIES YalahooCore  Qt6::Network
    TIMEOUT 60)

yalahoo_unit_test(JsonFileTest json-file LIBRARIES YalahooCore)

add_executable(NativeToolTest NativeToolTest.cpp)
target_include_directories(NativeToolTest PRIVATE "${PROJECT_SOURCE_DIR}/tests/support")
set_target_properties(NativeToolTest PROPERTIES AUTOMOC FALSE AUTOUIC FALSE)
yalahoo_test_tool(NativeToolTest)
add_test(NAME native-tool COMMAND NativeToolTest)
set_tests_properties(native-tool PROPERTIES TIMEOUT 10)

yalahoo_unit_test(RenderSupportTest render-support
    TIMEOUT 30
    LIBRARIES YalahooRhi Qt6::GuiPrivate
    INCLUDES "${PROJECT_SOURCE_DIR}/src/app" "${PROJECT_SOURCE_DIR}/tests/render"
    DEFINITIONS FIXTURE_ROOT="${PROJECT_SOURCE_DIR}"
    ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins" "QT_SCALE_FACTOR=1" "QT_QPA_PLATFORM=offscreen")

if(UNIX)
    add_test(NAME unix-deployment COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tests/tools/UnixDeploymentTest.py")
    add_test(NAME linux-package-tools COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tests/tools/LinuxDeploymentTest.py")
    set_tests_properties(unix-deployment linux-package-tools PROPERTIES TIMEOUT 60)
    add_test(NAME launchers COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tests/tools/LauncherTest.py")
    set_tests_properties(launchers PROPERTIES TIMEOUT 90)
endif()
add_test(NAME windows-deployment-report COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tests/tools/WindowsDeploymentTest.py")
set_tests_properties(windows-deployment-report PROPERTIES TIMEOUT 180 SKIP_RETURN_CODE 77)

yalahoo_unit_test(ApplicationOptionsTest application-options
    SOURCES "${PROJECT_SOURCE_DIR}/src/app/ApplicationOptions.cpp"
    INCLUDES "${PROJECT_SOURCE_DIR}/src/app"
    LIBRARIES YalahooCore)

yalahoo_unit_test(VerificationInputTest verification-input
    SOURCES "${PROJECT_SOURCE_DIR}/tools/assets/GeometryAudit.cpp"
    INCLUDES "${PROJECT_SOURCE_DIR}/tools/assets" "${PROJECT_SOURCE_DIR}/tools/migration" "${PROJECT_SOURCE_DIR}/src/core"
    LIBRARIES Qt6::Gui)

yalahoo_unit_test(ReadbackQueueTest readback-queue
    LIBRARIES YalahooRhi Qt6::GuiPrivate)
