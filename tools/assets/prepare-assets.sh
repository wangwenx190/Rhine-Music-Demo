#!/bin/sh
set -eu
repository=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
preset=${1:-qt-release}
case "$preset" in
    qt-debug|qt-release)
        ;;
    *)
        echo 'Expected qt-debug or qt-release' >&2
        exit 2
        ;;
esac
cd "$repository"
cmake --preset "$preset"
cmake --build --preset "$preset" --target prepare-assets
build="build/${preset#qt-}"
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
        echo 'Use prepare-assets.ps1 on Windows' >&2
        exit 2
        ;;
esac
"$build/tools/prepare-assets" "$repository" "$kit"
# Font container conversion is Windows-only. The C++ tool checks both source
# and prepared SFNT hashes before reusing the portable checked-in outputs.
