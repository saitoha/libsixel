#!/bin/sh
# Emit TAP for binding loader_setopt parity check.

set -eu

echo "1..1"

src_root=$1

if sh "$src_root/tests/bindings/check-loader-setopt-sync.sh" "$src_root" >/dev/null 2>&1; then
    echo "ok 1 - binding loader_setopt case sync"
    exit 0
fi

echo "not ok 1 - binding loader_setopt case sync"
sh "$src_root/tests/bindings/check-loader-setopt-sync.sh" "$src_root" 2>&1 | sed 's/^/# /'
exit 1
