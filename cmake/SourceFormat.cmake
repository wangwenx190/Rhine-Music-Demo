include_guard(GLOBAL)

# Probe PATH and the active MSVC installation without fixing a machine path.
cmake_path(GET CMAKE_CXX_COMPILER PARENT_PATH compiler_directory)
find_program(
    YALAHOO_CLANG_FORMAT
    NAMES
    clang-format
    HINTS
    "${compiler_directory}/../../../../../Llvm/x64/bin"
    "${compiler_directory}/../../../../../Llvm/ARM64/bin"
)
yalahoo_check_tool(qmlformat "--version")

set(
    format_command
    "${CMAKE_COMMAND}"
    "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
    "-DCLANG_FORMAT=${YALAHOO_CLANG_FORMAT}"
    "-DQML_FORMAT=${YALAHOO_qmlformat}"
    -P
    "${PROJECT_SOURCE_DIR}/tools/format/NativeFormat.cmake"
)
add_custom_target(
    format
    COMMAND
    ${format_command}
    COMMENT
    "Format native sources with Qt style"
    VERBATIM
)
add_custom_target(
    check-format
    COMMAND
    ${format_command}
    --check
    COMMENT
    "Check native sources against Qt style"
    VERBATIM
)
add_test(NAME source-format COMMAND ${format_command} --check)
set_tests_properties(source-format PROPERTIES TIMEOUT 120)
