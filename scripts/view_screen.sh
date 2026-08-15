#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
pio_python=$(pio system info | sed -n 's/^Python Executable  *//p')

if [ -z "$pio_python" ]; then
  echo "Could not locate PlatformIO's Python environment" >&2
  exit 1
fi

exec "$pio_python" "$project_root/tools/screen_viewer.py" "$@"
