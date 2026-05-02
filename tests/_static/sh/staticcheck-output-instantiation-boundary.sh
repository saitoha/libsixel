#!/bin/sh
# Emit TAP for output construction boundary checks.

set -eu

src_root=${1:-}

echo "1..1"

if test -z "$src_root"; then
    echo "not ok 1 - src output construction uses terminal/output factory"
    echo "# src_root argument is required"
    exit 1
fi

if test ! -d "$src_root/src"; then
    echo "not ok 1 - src output construction uses terminal/output factory"
    echo "# missing source directory: $src_root/src"
    exit 1
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-output-boundary-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

violations=$tmpdir/direct-output-new.txt

find "$src_root/src" -type f -name '*.c' ! -name 'output.c' -exec awk '
/sixel_output_new[[:space:]]*\(/ {
    print FILENAME ":" FNR ":" $0
}
' {} + > "$violations"

if test -s "$violations"; then
    echo "not ok 1 - src output construction uses terminal/output factory"
    sed 's/^/# direct constructor: /' "$violations"
    exit 1
fi

echo "ok 1 - src output construction uses terminal/output factory"
exit 0
