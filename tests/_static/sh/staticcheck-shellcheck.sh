#!/bin/sh
# Emit TAP for shellcheck static check.

set -eu

shellcheck_driver=$1
src_root=$2
shellcheck_bin=$3

if test -z "$shellcheck_bin"; then
    echo "1..0 # SKIP shellcheck not found"
    exit 0
fi

if test ! -x "$shellcheck_bin" && ! command -v "$shellcheck_bin" >/dev/null 2>&1; then
    echo "1..0 # SKIP shellcheck executable not found: $shellcheck_bin"
    exit 0
fi

echo "1..1"

if SHELLCHECK_CMD="$shellcheck_bin" "$shellcheck_driver" "$src_root"; then
    echo "ok 1 - shellcheck"
else
    echo "not ok 1 - shellcheck"
    exit 1
fi
