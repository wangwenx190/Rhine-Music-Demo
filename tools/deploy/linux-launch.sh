#!/bin/sh
set -eu
package=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ ! -x "$package/bin/yalahoo" ] || [ ! -f "$package/package.json" ] || [ ! -f "$package/bin/qt.conf" ]; then
    echo 'Incomplete Linux package; run tools/deploy/linux.sh.' >&2
    exit 2
fi
# Keep session, display, XDG and audio routing. Only library/import overrides
# belonging to another Qt installation are removed from this child process.
unset QT_PLUGIN_PATH QT_QPA_PLATFORM_PLUGIN_PATH QT_CONF_PATH QT_QPA_PLATFORMTHEME
unset QML_IMPORT_PATH QML2_IMPORT_PATH LD_PRELOAD LD_AUDIT
LD_LIBRARY_PATH="$package/lib"
export LD_LIBRARY_PATH
exec "$package/bin/yalahoo" "$@"
