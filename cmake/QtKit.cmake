include_guard(GLOBAL)

set(
    YALAHOO_QT_COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickTemplates2
    QuickDialogs2
    Quick3D
    Multimedia
    Concurrent
    Svg
    ShaderTools
    CanvasPainter
    Test
    QuickTest
    LinguistTools
)

# Records: prefix|version|system|architecture|ABI|missing modules. Kept independent
# of discovery so selection and failure paths can be tested without installing Qt.
function(yalahoo_choose_kit records system architecture abi out error)
    set(latest "0")
    set(candidates)
    foreach(record IN LISTS records)
        string(REPLACE "|" ";" fields "${record}")
        list(GET fields 1 version)
        list(GET fields 2 platform)
        list(GET fields 3 arch)
        if(platform STREQUAL system AND arch STREQUAL architecture)
            if(version VERSION_GREATER latest)
                set(latest "${version}")
                set(candidates "${record}")
            elseif(version VERSION_EQUAL latest)
                list(APPEND candidates "${record}")
            endif()
        endif()
    endforeach()
    set(chosen "")
    set(problem "No native desktop Qt Kit for ${system}/${architecture}")
    foreach(record IN LISTS candidates)
        string(REPLACE "|" ";" fields "${record}")
        list(GET fields 4 kit_abi)
        if(kit_abi STREQUAL abi)
            list(GET fields 0 prefix)
            list(GET fields 5 missing)
            if(NOT missing STREQUAL "none")
                set(problem "Latest Qt ${latest} is missing modules: ${missing}; no older fallback")
            elseif(chosen STREQUAL "")
                set(chosen "${prefix}")
                set(problem "")
                break()
            endif()
        elseif(chosen STREQUAL "")
            set(
                problem
                "Latest Qt ${latest} ABI ${kit_abi} does not match ${abi}; no older fallback"
            )
        endif()
    endforeach()
    set(${out} "${chosen}" PARENT_SCOPE)
    set(${error} "${problem}" PARENT_SCOPE)
endfunction()

function(yalahoo_validate_override selected override error)
    set(problem "")
    if(override AND NOT override STREQUAL selected)
        set(
            problem
            "Qt override/cache '${override}' differs from latest Kit '${selected}'. Reconfigure with --fresh and remove stale overrides."
        )
    endif()
    set(${error} "${problem}" PARENT_SCOPE)
endfunction()

# Qt's macOS universal Kit can have QT_ARCH=x86_64 while QT_ARCHS includes
# arm64. Keep the application native, but accept either slice of that Kit.
function(yalahoo_kit_architectures pri system out)
    set(architectures)
    if(pri MATCHES "(^|\n)[ \t]*QT_ARCH[ \t]*=[ \t]*([^\r\n ]+)")
        set(primary "${CMAKE_MATCH_2}")
        set(declared "${primary}")
        if(system STREQUAL "Darwin" AND pri MATCHES "(^|\n)[ \t]*QT_ARCHS[ \t]*=[ \t]*([^\r\n]+)")
            set(declared "${CMAKE_MATCH_2}")
            string(REPLACE "$$QT_ARCH" "${primary}" declared "${declared}")
        endif()
        string(STRIP "${declared}" declared)
        string(REGEX REPLACE "[ \t]+" ";" declared "${declared}")
        foreach(value IN LISTS declared)
            string(TOLOWER "${value}" value)
            if(value MATCHES "^(amd64|x86_64|x64)$")
                list(APPEND architectures x86_64)
            elseif(value MATCHES "^(arm64|aarch64)$")
                list(APPEND architectures arm64)
            endif()
        endforeach()
        list(REMOVE_DUPLICATES architectures)
    endif()
    set(${out} "${architectures}" PARENT_SCOPE)
endfunction()

function(yalahoo_discover_qt)
    if(CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR "Only native desktop Kits are supported")
    endif()
    if(WIN32)
        set(system Windows)
        if(NOT MSVC)
            message(FATAL_ERROR "Windows requires the native MSVC ABI")
        endif()
        set(abi msvc)
        set(default_roots "C:/Qt" "$ENV{USERPROFILE}/Qt")
    elseif(APPLE)
        set(system Darwin)
        set(abi clang)
        set(default_roots "$ENV{HOME}/Qt" "/opt/Qt" "/usr/local/Qt")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(system Linux)
        set(abi gcc)
        set(default_roots "$ENV{HOME}/Qt" "/opt/Qt" "/usr/local/Qt" "/usr")
    else()
        message(FATAL_ERROR "Unsupported desktop platform: ${CMAKE_SYSTEM_NAME}")
    endif()
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" arch)
    if(WIN32 AND CMAKE_CXX_COMPILER_ARCHITECTURE_ID)
        string(TOLOWER "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}" arch)
    endif()
    if(arch MATCHES "^(amd64|x86_64|x64)$")
        set(arch x86_64)
    elseif(arch MATCHES "^(arm64|aarch64)$")
        set(arch arm64)
    else()
        message(FATAL_ERROR "Unsupported native architecture: ${arch}")
    endif()
    if(APPLE AND CMAKE_OSX_ARCHITECTURES AND NOT CMAKE_OSX_ARCHITECTURES STREQUAL arch)
        message(
            FATAL_ERROR
            "R02 requires a single native macOS architecture; use the host architecture ${arch}"
        )
    endif()
    set(roots ${YALAHOO_QT_ROOTS} $ENV{YALAHOO_QT_ROOTS} ${default_roots})
    if(YALAHOO_QT_KIT)
        list(APPEND roots "${YALAHOO_QT_KIT}")
    endif()
    if(DEFINED ENV{YALAHOO_QT_KIT})
        list(APPEND roots "$ENV{YALAHOO_QT_KIT}")
    endif()
    set(configs)
    foreach(root IN LISTS roots)
        file(
            GLOB
            found
            "${root}/lib/cmake/Qt6/Qt6Config.cmake"
            "${root}/*/lib/cmake/Qt6/Qt6Config.cmake"
            "${root}/*/*/lib/cmake/Qt6/Qt6Config.cmake"
            "${root}/lib/*/cmake/Qt6/Qt6Config.cmake"
        )
        list(APPEND configs ${found})
    endforeach()
    list(REMOVE_DUPLICATES configs)
    set(records)
    foreach(config IN LISTS configs)
        get_filename_component(config_dir "${config}" DIRECTORY)
        file(READ "${config_dir}/Qt6ConfigVersionImpl.cmake" version_text)
        if(NOT version_text MATCHES "set\\(PACKAGE_VERSION \"([0-9]+\\.[0-9]+\\.[0-9]+)\"\\)")
            message(FATAL_ERROR "Cannot read actual Qt version: ${config}")
        endif()
        set(version "${CMAKE_MATCH_1}")
        # Qt relocatable CMake configs are normally prefix/lib/cmake/Qt6.
        get_filename_component(prefix "${config_dir}/../../.." ABSOLUTE)
        if(NOT EXISTS "${prefix}/mkspecs/qconfig.pri")
            # Distribution multiarch layout: prefix/lib/<arch>/cmake/Qt6.
            get_filename_component(prefix "${prefix}/.." ABSOLUTE)
        endif()
        if(NOT EXISTS "${prefix}/mkspecs/qconfig.pri")
            continue()
        endif()
        file(READ "${prefix}/mkspecs/qconfig.pri" pri)
        if(pri MATCHES "enabled_features[^\n]*cross_compile")
            continue()
        endif()
        unset(qmake)
        find_program(qmake NAMES qmake qmake6 PATHS "${prefix}/bin" NO_DEFAULT_PATH NO_CACHE)
        if(NOT qmake)
            message(FATAL_ERROR "Kit has no matching qmake: ${prefix}")
        endif()
        execute_process(
            COMMAND
            "${qmake}"
            -query
            QMAKE_XSPEC
            OUTPUT_VARIABLE
            spec
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE
            query_result
        )
        if(NOT query_result EQUAL 0)
            message(FATAL_ERROR "qmake Kit probe failed: ${prefix}")
        endif()
        if(spec MATCHES "^win32-msvc")
            set(kit_system Windows)
            set(kit_abi msvc)
        elseif(spec MATCHES "^macx-clang")
            set(kit_system Darwin)
            set(kit_abi clang)
        elseif(spec MATCHES "^linux-(g\\+\\+|clang)")
            set(kit_system Linux)
            set(kit_abi gcc)
        else()
            continue()
        endif()
        yalahoo_kit_architectures("${pri}" "${kit_system}" kit_architectures)
        if(NOT kit_architectures)
            continue()
        endif()
        execute_process(
            COMMAND
            "${qmake}"
            -query
            QT_VERSION
            OUTPUT_VARIABLE
            qmake_version
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE
            query_result
        )
        if(NOT query_result EQUAL 0 OR NOT qmake_version STREQUAL version)
            message(FATAL_ERROR "Qt CMake/qmake version mismatch in ${prefix}")
        endif()
        set(missing)
        foreach(module IN LISTS YALAHOO_QT_COMPONENTS)
            if(NOT EXISTS "${config_dir}/../Qt6${module}/Qt6${module}Config.cmake")
                list(APPEND missing "${module}")
            endif()
        endforeach()
        if(missing)
            string(JOIN "," missing ${missing})
        else()
            set(missing none)
        endif()
        foreach(kit_arch IN LISTS kit_architectures)
            list(APPEND records "${prefix}|${version}|${kit_system}|${kit_arch}|${kit_abi}|${missing}")
        endforeach()
    endforeach()
    yalahoo_choose_kit("${records}" "${system}" "${arch}" "${abi}" selected problem)
    if(problem)
        message(FATAL_ERROR "${problem}")
    endif()
    foreach(override IN ITEMS "${YALAHOO_SELECTED_QT}" "${YALAHOO_QT_KIT}" "$ENV{YALAHOO_QT_KIT}")
        if(override)
            file(TO_CMAKE_PATH "${override}" override)
            yalahoo_validate_override("${selected}" "${override}" problem)
            if(problem)
                message(FATAL_ERROR "${problem}")
            endif()
        endif()
    endforeach()
    foreach(record IN LISTS records)
        string(REPLACE "|" ";" fields "${record}")
        list(GET fields 0 prefix)
        if(prefix STREQUAL selected)
            list(GET fields 1 version)
            break()
        endif()
    endforeach()
    set(
        YALAHOO_SELECTED_QT
        "${selected}"
        CACHE
        PATH
        "Automatically discovered latest native Kit"
        FORCE
    )
    set(YALAHOO_QT_VERSION "${version}" CACHE INTERNAL "Actual Kit version" FORCE)
    # Reject cached module directories, including a stale manually supplied Qt6_DIR.
    get_cmake_property(cache_variables CACHE_VARIABLES)
    foreach(variable IN LISTS cache_variables)
        if(variable MATCHES "^Qt6.*_DIR$")
            if(${variable} AND NOT "${${variable}}" STREQUAL "${variable}-NOTFOUND")
                cmake_path(IS_PREFIX selected "${${variable}}" NORMALIZE inside_kit)
                if(NOT inside_kit)
                    message(FATAL_ERROR "Stale ${variable}; reconfigure with --fresh")
                endif()
            endif()
        endif()
    endforeach()
    list(PREPEND CMAKE_PREFIX_PATH "${selected}")
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)
    message(STATUS "Latest native Qt ${version}: ${selected} (${system}/${arch}/${abi})")
endfunction()

function(yalahoo_check_tool name arguments)
    find_program(
        tool
        NAMES
        "${name}"
        PATHS
        "${YALAHOO_SELECTED_QT}/bin"
        "${YALAHOO_SELECTED_QT}/libexec"
        NO_DEFAULT_PATH
        NO_CACHE
        REQUIRED
    )
    execute_process(
        COMMAND
        "${tool}"
        ${arguments}
        OUTPUT_VARIABLE
        stdout
        ERROR_VARIABLE
        stderr
        RESULT_VARIABLE
        result
    )
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" tool_version "${stdout}${stderr}")
    if(NOT result EQUAL 0 OR NOT tool_version STREQUAL YALAHOO_QT_VERSION)
        message(
            FATAL_ERROR
            "${name} must match selected Qt ${YALAHOO_QT_VERSION}: ${stdout}${stderr}"
        )
    endif()
    set(YALAHOO_${name} "${tool}" CACHE INTERNAL "Selected Kit tool" FORCE)
endfunction()
