#!/bin/sh
# Emit TAP for shellcheck static check.

set -eu

echo "1..1"

shellcheck_driver=$1
src_root=$2
shellcheck_bin=$3

SHELLCHECK_CMD="$shellcheck_bin" "$shellcheck_driver" "$src_root" >/dev/null
echo "ok 1 - shellcheck"
