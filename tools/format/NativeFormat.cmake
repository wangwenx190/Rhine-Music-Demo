cmake_minimum_required(VERSION 3.25)

if(NOT EXISTS "${CLANG_FORMAT}" OR NOT EXISTS "${QML_FORMAT}")
    message(
        FATAL_ERROR
        "Formatting requires clang-format and qmlformat from the selected Qt Kit. Install/locate clang-format and reconfigure; YALAHOO_CLANG_FORMAT may specify its executable."
    )
endif()

set(check FALSE)
foreach(index RANGE 0 ${CMAKE_ARGC})
    if("${CMAKE_ARGV${index}}" STREQUAL "--check")
        set(check TRUE)
    endif()
endforeach()

set(cpp_files)
set(qml_files)
foreach(directory IN ITEMS src qml tests tools resources/shaders)
    file(
        GLOB_RECURSE
        sources
        LIST_DIRECTORIES
        FALSE
        "${SOURCE_ROOT}/${directory}/*.cpp"
        "${SOURCE_ROOT}/${directory}/*.h"
        "${SOURCE_ROOT}/${directory}/*.hpp"
        "${SOURCE_ROOT}/${directory}/*.c"
        "${SOURCE_ROOT}/${directory}/*.cc"
        "${SOURCE_ROOT}/${directory}/*.cxx"
        "${SOURCE_ROOT}/${directory}/*.vert"
        "${SOURCE_ROOT}/${directory}/*.frag"
        "${SOURCE_ROOT}/${directory}/*.glsl"
        "${SOURCE_ROOT}/${directory}/*.mjs"
        "${SOURCE_ROOT}/${directory}/*.js"
    )
    list(APPEND cpp_files ${sources})
    file(GLOB_RECURSE sources LIST_DIRECTORIES FALSE "${SOURCE_ROOT}/${directory}/*.qml")
    list(APPEND qml_files ${sources})
endforeach()

set(failures)
foreach(source IN LISTS cpp_files)
    # GLSL uses C-like syntax; select clang-format's C++ parser explicitly.
    if(check)
        set(arguments --dry-run --Werror)
    else()
        set(arguments -i)
    endif()
    if(source MATCHES "\\.m?js$")
        set(language_file source.js)
    else()
        set(language_file source.cpp)
    endif()
    execute_process(
        COMMAND
        "${CLANG_FORMAT}"
        "--style=file:${SOURCE_ROOT}/.clang-format"
        "--assume-filename=${language_file}"
        ${arguments}
        "${source}"
        RESULT_VARIABLE
        result
        OUTPUT_VARIABLE
        output
        ERROR_VARIABLE
        error
        ENCODING
        UTF-8
    )
    if(NOT result EQUAL 0)
        list(APPEND failures "${source}: ${output}${error}")
    endif()
endforeach()

foreach(source IN LISTS qml_files)
    if(check)
        execute_process(
            COMMAND
            "${QML_FORMAT}"
            -s
            "${SOURCE_ROOT}/.qmlformat.ini"
            "${source}"
            RESULT_VARIABLE
            result
            OUTPUT_VARIABLE
            formatted
            ERROR_VARIABLE
            error
            ENCODING
            UTF-8
        )
        file(READ "${source}" original)
        string(REPLACE "\r\n" "\n" original "${original}")
        string(REPLACE "\r\n" "\n" formatted "${formatted}")
        if(result EQUAL 0 AND NOT original STREQUAL formatted)
            list(APPEND failures "${source}: run the format target")
        endif()
    else()
        execute_process(
            COMMAND
            "${QML_FORMAT}"
            -s
            "${SOURCE_ROOT}/.qmlformat.ini"
            -i
            "${source}"
            RESULT_VARIABLE
            result
            OUTPUT_VARIABLE
            output
            ERROR_VARIABLE
            error
            ENCODING
            UTF-8
        )
    endif()
    if(NOT result EQUAL 0)
        list(APPEND failures "${source}: ${error}")
    endif()
endforeach()

if(failures)
    list(JOIN failures "\n" diagnostics)
    message(FATAL_ERROR "Qt source formatting failed:\n${diagnostics}")
endif()
list(LENGTH cpp_files cpp_count)
list(LENGTH qml_files qml_count)
message(
    STATUS
    "Qt style: ${cpp_count} C++/shader/JavaScript files, ${qml_count} QML files (check=${check})"
)
