#!/bin/sh
set -eu
launcher=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec "$launcher/lighting-lab.sh" "$@"
