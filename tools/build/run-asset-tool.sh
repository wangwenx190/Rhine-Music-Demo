#!/bin/sh
# Build and launch a native asset tool with the exact Kit selected by CMake.
set -eu
repository=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
tool=$1
shift
case "$tool" in
    prepare-assets|export-branding)
        ;;
    *)
        echo 'Unknown native asset tool' >&2
        exit 2
        ;;
esac
preset=${1:-qt-release}
case "$preset" in
    qt-debug|qt-release)
        ;;
    *)
        echo 'Expected qt-debug or qt-release' >&2
        exit 2
        ;;
esac
if [ "$#" -gt 0 ]; then
    shift
fi
cd "$repository"
cmake --preset "$preset"
cmake --build --preset "$preset" --target "$tool"
build="$repository/build/${preset#qt-}"
kit=$(sed -n 's/^YALAHOO_SELECTED_QT:PATH=//p' "$build/CMakeCache.txt")
test -n "$kit"
export QT_PLUGIN_PATH="$kit/plugins"
case "$(uname -s)" in
    Darwin)
        export DYLD_LIBRARY_PATH="$kit/lib${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
        ;;
    Linux)
        export LD_LIBRARY_PATH="$kit/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
        ;;
    *)
        echo 'Use the native PowerShell asset entry on Windows' >&2
        exit 2
        ;;
esac
if [ "$tool" = prepare-assets ]; then
    exec "$build/tools/$tool" "$repository" "$kit" "$@"
fi
exec "$build/tools/$tool" "$repository" "$@"
