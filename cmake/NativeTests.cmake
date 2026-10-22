include_guard(GLOBAL)

# Declare ordinary Qt unit executables; resource/QML/probe-specific setup stays at the call site.
function(yalahoo_unit_test target name)
    cmake_parse_arguments(PARSE_ARGV 2 test "SERIAL" "TIMEOUT;REPORT;WORKING_DIRECTORY" "LIBRARIES;DEFINITIONS;SOURCES;INCLUDES;ENVIRONMENT")
    if(test_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown unit test arguments: ${test_UNPARSED_ARGUMENTS}")
    endif()
    qt_add_executable(${target} "${target}.cpp" ${test_SOURCES})
    target_link_libraries(${target} PRIVATE ${test_LIBRARIES} Qt6::Test)
    if(test_DEFINITIONS)
        target_compile_definitions(${target} PRIVATE ${test_DEFINITIONS})
    endif()
    if(test_INCLUDES)
        target_include_directories(${target} PRIVATE ${test_INCLUDES})
    endif()
    yalahoo_platform(${target})
    if(test_REPORT)
        add_test(NAME ${name} COMMAND ${target}
            -o "${CMAKE_BINARY_DIR}/verification/${test_REPORT},junitxml" -o -,txt)
    else()
        add_test(NAME ${name} COMMAND ${target})
    endif()
    foreach(property IN ITEMS ENVIRONMENT WORKING_DIRECTORY)
        if(test_${property})
            set_tests_properties(${name} PROPERTIES ${property} "${test_${property}}")
        endif()
    endforeach()
    if(test_TIMEOUT)
        set_tests_properties(${name} PROPERTIES TIMEOUT ${test_TIMEOUT})
    endif()
    if(test_SERIAL)
        set_tests_properties(${name} PROPERTIES RUN_SERIAL TRUE)
    endif()
endfunction()

# UI hosts share warnings, scale and plugin policy; component-specific behavior stays in arguments.
function(yalahoo_ui_test name option directory timeout)
    cmake_parse_arguments(PARSE_ARGV 4 ui "THREADED" "" "ARGUMENTS")
    if(ui_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown UI test arguments: ${ui_UNPARSED_ARGUMENTS}")
    endif()
    add_test(NAME ${name} COMMAND ui-tests ${option}
        "${CMAKE_BINARY_DIR}/verification/${directory}" ${ui_ARGUMENTS})
    set_tests_properties(${name} PROPERTIES TIMEOUT ${timeout}
        ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1"
        FAIL_REGULAR_EXPRESSION "Binding loop;Cannot assign;ReferenceError;TypeError;Failed to compile")
    if(ui_THREADED)
        set_property(TEST ${name} APPEND PROPERTY ENVIRONMENT "QSG_RENDER_LOOP=threaded")
    endif()
endfunction()

# Regression executables are distinct from asset generators and audit programs.
function(yalahoo_test_tool target)
    yalahoo_tool(${target})
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
endfunction()

# Isolated material and lighting checks share the same source-fixture protocol.
function(yalahoo_material_test name fixture report timeout)
    add_test(NAME ${name} COMMAND Yalahoo --verify-materials
        "${PROJECT_SOURCE_DIR}/tests/fixtures/${fixture}"
        --material-report "${CMAKE_BINARY_DIR}/verification/${report}")
    set_tests_properties(${name} PROPERTIES TIMEOUT ${timeout}
        ENVIRONMENT "QT_ENABLE_HIGHDPI_SCALING=0;QT_SCALE_FACTOR=1;QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins"
        FAIL_REGULAR_EXPRESSION "Binding loop;Cannot assign;ReferenceError;TypeError;Failed to compile;QSpirvCompiler")
endfunction()
