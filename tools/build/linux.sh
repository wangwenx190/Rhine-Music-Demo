#!/bin/sh
set -eu
if [ "$(uname -s)" != Linux ]; then
    echo 'Linux build requires a native Linux environment.' >&2
    exit 2
fi
script_directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec "$script_directory/build.sh" qt-release "$@"
