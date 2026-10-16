#!/bin/sh
set -eu
repository=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
case "$(uname -s)" in
    Darwin)
        package="$repository/out/macos"
        program="$package/yalahoo.app/Contents/MacOS/yalahoo"
        if [ -x "$program" ]; then
            if [ ! -f "$package/package.json" ] || [ ! -d "$package/yalahoo.app/Contents/Frameworks" ]; then
                echo 'Incomplete macOS package; run tools/deploy/macos.sh.' >&2
                exit 2
            fi
            unset QT_PLUGIN_PATH QT_QPA_PLATFORM_PLUGIN_PATH QT_CONF_PATH
            unset QML_IMPORT_PATH QML2_IMPORT_PATH DYLD_LIBRARY_PATH DYLD_FRAMEWORK_PATH
            unset DYLD_FALLBACK_LIBRARY_PATH DYLD_FALLBACK_FRAMEWORK_PATH DYLD_INSERT_LIBRARIES
            exec "$program" "$@"
        fi
        program="$repository/build/release/bin/yalahoo.app/Contents/MacOS/yalahoo"
        ;;
    Linux)
        package="$repository/out/linux"
        if [ -d "$package" ]; then
            if [ ! -x "$package/yalahoo.sh" ] || [ ! -f "$package/package.json" ]; then
                echo 'Incomplete Linux package; run tools/deploy/linux.sh.' >&2
                exit 2
            fi
            exec "$package/yalahoo.sh" "$@"
        fi
        program="$repository/build/release/bin/yalahoo"
        ;;
    *)
        echo 'Unsupported native platform.' >&2
        exit 2
        ;;
esac
if [ ! -x "$program" ]; then
    echo 'Build the native Release application first (cmake --preset qt-release).' >&2
    exit 2
fi
cache="$repository/build/release/CMakeCache.txt"
if [ ! -f "$cache" ]; then
    echo 'Configure the native Release Qt Kit first.' >&2
    exit 2
fi
configuration=$(sed -n 's/^CMAKE_BUILD_TYPE:STRING=//p' "$cache")
if [ "$configuration" != Release ]; then
    echo 'The native launcher requires a Release build.' >&2
    exit 2
fi
kit=$(sed -n 's/^YALAHOO_SELECTED_QT:PATH=//p' "$cache")
if [ -z "$kit" ] || [ ! -d "$kit/plugins" ]; then
    echo 'The configured native Qt Kit is unavailable; reconfigure Release.' >&2
    exit 2
fi
unset QT_QPA_PLATFORM_PLUGIN_PATH QT_CONF_PATH QML2_IMPORT_PATH
QT_PLUGIN_PATH="$kit/plugins"
QML_IMPORT_PATH="$kit/qml"
export QML_IMPORT_PATH
if [ "$(uname -s)" = Linux ]; then
    LD_LIBRARY_PATH="$kit/lib"
    export LD_LIBRARY_PATH
fi
export QT_PLUGIN_PATH
exec "$program" "$@"
