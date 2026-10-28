#!/bin/sh
set -eu
repository=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
preset=${1:-qt-release}
case "$preset" in
    qt-debug|qt-release)
        ;;
    *)
        echo 'Use qt-debug or qt-release' >&2
        exit 2
        ;;
esac
cd "$repository"
# Native compiler, CMake and Ninja must already be installed. Kit discovery is
# shared with Windows in cmake/QtKit.cmake; no PATH-based Qt tool selection.
if [ "$#" -gt 0 ]; then
    shift
fi
cmake --preset "$preset" "$@"
cmake --build --preset "$preset"
# Select affected tests explicitly according to docs/contracts/test-gates.md.
