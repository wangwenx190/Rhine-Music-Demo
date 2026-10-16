#!/bin/sh
set -eu
repository=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
case "$(uname -s)" in
    Darwin)
        program="$repository/build/release/bin/yalahoo.app/Contents/MacOS/yalahoo"
        ;;
    *)
        program="$repository/build/release/bin/yalahoo"
        ;;
esac
if [ ! -x "$program" ]; then
    echo 'Build the native Release application first (cmake --preset qt-release).' >&2
    exit 2
fi
kit=$(sed -n 's/^YALAHOO_SELECTED_QT:PATH=//p' "$repository/build/release/CMakeCache.txt")
if [ -z "$kit" ] || [ ! -d "$kit/plugins" ]; then
    echo 'The configured native Qt Kit is unavailable; reconfigure Release.' >&2
    exit 2
fi
QT_PLUGIN_PATH="$kit/plugins"
export QT_PLUGIN_PATH
exec "$program" "$@"
