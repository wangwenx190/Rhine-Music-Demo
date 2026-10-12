#!/bin/sh
set -eu
repository=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
preset=${1:-qt-release}
case "$preset" in
    qt-debug)
        configuration=debug
        ;;
    qt-release)
        configuration=release
        ;;
    *)
        echo 'Expected qt-debug or qt-release' >&2
        exit 2
        ;;
esac
shift "$(( $# > 0 ? 1 : 0 ))"
cd "$repository"
cmake --preset "$preset"
cmake --build --preset "$preset" --target export-branding
# Build Kit discovery supplies the target's runtime linkage, never an arbitrary
# Qt from PATH. On macOS CMake uses the same Kit's framework install names.
exec "$repository/build/$configuration/tools/export-branding" "$repository" "$@"
