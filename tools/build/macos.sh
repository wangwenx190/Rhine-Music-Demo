#!/bin/sh
set -eu
if [ "$(uname -s)" != Darwin ]; then
    echo 'The macOS build entry requires native macOS.' >&2
    exit 2
fi
script_directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec sh "$script_directory/build.sh" qt-release "$@"
