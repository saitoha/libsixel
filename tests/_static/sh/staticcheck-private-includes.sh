#!/bin/sh
# Emit TAP for private include validation.

set -eu

src_root=$1
python_bin=${2:-}

if test -z "$python_bin"; then
    if command -v python3 >/dev/null 2>&1; then
        python_bin=python3
    elif command -v python >/dev/null 2>&1; then
        python_bin=python
    else
        echo "1..0 # SKIP python interpreter not found"
        exit 0
    fi
fi

if test ! -x "$python_bin" && ! command -v "$python_bin" >/dev/null 2>&1; then
    echo "1..0 # SKIP python executable not found: $python_bin"
    exit 0
fi

echo "1..1"

cd "$src_root" || {
    echo "not ok 1 - private include check"
    exit 1
}

if "$python_bin" "$src_root/tools/check_private_includes.py"; then
    echo "ok 1 - private include check"
else
    echo "not ok 1 - private include check"
    exit 1
fi
