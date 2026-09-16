#!/bin/sh
# Emit TAP for executable shell test sources.
# Policy: docs/testing/guide.md

set -eu

src_root=$1
python_bin=${2:-python3}

if ! command -v "$python_bin" >/dev/null 2>&1; then
    echo "1..0 # SKIP Python interpreter unavailable"
    exit 0
fi

status=0
"$python_bin" "$src_root/tools/check_tap_executable.py" "$src_root" || status=$?
if test "$status" -eq 77; then
    echo "1..0 # SKIP executable modes unavailable without Git on this platform"
    exit 0
fi

echo "1..1"
if test "$status" -ne 0; then
    echo "not ok 1 - shell TAP sources are executable"
    exit 1
fi
echo "ok 1 - shell TAP sources are executable"
