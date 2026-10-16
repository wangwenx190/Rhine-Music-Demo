#!/bin/sh
set -eu
launcher=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec "$launcher/run-native.sh" --lighting-lab "$@"
