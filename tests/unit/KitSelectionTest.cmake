include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/QtKit.cmake")
function(expect_architectures pri system expected)
    yalahoo_kit_architectures("${pri}" "${system}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Expected Kit architectures '${expected}', got '${actual}'")
    endif()
endfunction()
expect_architectures("QT_ARCH = x86_64\nQT_ARCHS = x86_64 arm64\n" Darwin "x86_64;arm64")
expect_architectures("QT_ARCH = aarch64\nQT_ARCHS = $$QT_ARCH\n" Darwin arm64)
expect_architectures("QT_ARCH = x86_64\nQT_ARCHS = x86_64 arm64\n" Windows x86_64)
expect_architectures("QT_HOST_ARCH = x86_64\nQT_ARCH = arm64\n" Darwin arm64)
expect_architectures("QT_HOST_ARCH = arm64\n" Darwin "")
expect_architectures("QT_ARCH = arm64\nQT_ARCHS = arm64 arm64\n" Darwin arm64)
function(expect records system arch abi expected error_pattern)
    yalahoo_choose_kit("${records}" "${system}" "${arch}" "${abi}" chosen problem)
    if(NOT chosen STREQUAL expected)
        message(FATAL_ERROR "Expected '${expected}', got '${chosen}': ${problem}")
    endif()
    if(error_pattern AND NOT problem MATCHES "${error_pattern}")
        message(FATAL_ERROR "Expected diagnostic '${error_pattern}', got '${problem}'")
    elseif(NOT error_pattern AND problem)
        message(FATAL_ERROR "Unexpected diagnostic: ${problem}")
    endif()
endfunction()
set(old "/old|6.9.9|Windows|x86_64|msvc|none")
set(new "/new|6.10.2|Windows|x86_64|msvc|none")
expect("${new};${old}" Windows x86_64 msvc /new "")
expect(
    "${old};/android|9.0.0|Android|arm64|clang|none;/wasm|9.0.0|Wasm|wasm32|clang|none"
    Windows
    x86_64
    msvc
    /old
    ""
)
expect("${old};/arm|8.0.0|Windows|arm64|msvc|none" Windows x86_64 msvc /old "")
expect(
    "${old};/mingw|7.0.0|Windows|x86_64|mingw|none"
    Windows
    x86_64
    msvc
    ""
    "ABI.*no older fallback"
)
expect(
    "${old};/broken|7.0.0|Windows|x86_64|msvc|Quick3D,Multimedia"
    Windows
    x86_64
    msvc
    ""
    "missing modules.*no older fallback"
)
expect(
    "/mac|7.1.0|Darwin|arm64|clang|none;/linux|7.2.0|Linux|x86_64|gcc|none"
    Darwin
    arm64
    clang
    /mac
    ""
)
expect(
    "/mac|7.1.0|Darwin|arm64|clang|none;/linux|7.2.0|Linux|x86_64|gcc|none"
    Linux
    x86_64
    gcc
    /linux
    ""
)
expect("${old}" Linux x86_64 gcc "" "No native desktop")
expect(
    "/universal|7.2.0|Darwin|x86_64|clang|none;/universal|7.2.0|Darwin|arm64|clang|none;/intel|7.3.0|Darwin|x86_64|clang|none"
    Darwin
    arm64
    clang
    /universal
    ""
)
yalahoo_validate_override(/new /old error)
if(NOT error MATCHES "--fresh")
    message(FATAL_ERROR "Stale cache/override was accepted")
endif()
yalahoo_validate_override(/new /new error)
if(error)
    message(FATAL_ERROR "Current override rejected")
endif()
message(
    STATUS
    "Kit selection: numeric versions, native system/arch/ABI, missing modules, overrides/cache passed"
)
