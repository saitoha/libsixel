#!/bin/sh
# Emit TAP for the 6cells IDL generated-header sync contract.

set -eu

src_root=$1
idl_file=$src_root/include/6cells.idl
header_file=$src_root/include/6cells.h
generator=$src_root/tools/gen_6cells_h.awk

echo "1..1"

if test ! -f "$idl_file"; then
    echo "not ok 1 - 6cells.h stays generated from 6cells.idl"
    echo "# missing IDL file: $idl_file"
    exit 1
fi

if test ! -f "$header_file"; then
    echo "not ok 1 - 6cells.h stays generated from 6cells.idl"
    echo "# missing generated header: $header_file"
    exit 1
fi

if test ! -f "$generator"; then
    echo "not ok 1 - 6cells.h stays generated from 6cells.idl"
    echo "# missing generator: $generator"
    exit 1
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-6cells-idl-sync-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

expected=$tmpdir/6cells.expected.h

if ! awk -f "$generator" "$idl_file" >"$expected"; then
    echo "not ok 1 - 6cells.h stays generated from 6cells.idl"
    echo "# failed to regenerate include/6cells.h"
    exit 1
fi

if cmp -s "$header_file" "$expected"; then
    echo "ok 1 - 6cells.h stays generated from 6cells.idl"
    exit 0
fi

echo "not ok 1 - 6cells.h stays generated from 6cells.idl"
if command -v diff >/dev/null 2>&1; then
    diff -u "$header_file" "$expected" | sed 's/^/# /'
else
    echo "# diff not found"
fi
exit 1
