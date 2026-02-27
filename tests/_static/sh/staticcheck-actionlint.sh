#!/bin/sh
# Emit TAP for actionlint static check.

set -eu

echo "1..1"

src_root=$1
actionlint_bin=$2

if test -n "$actionlint_bin"; then
    cd "$src_root"
    "$actionlint_bin"
    echo "ok 1 - actionlint"
else
    echo "ok 1 # SKIP actionlint not found"
fi
