#!/bin/sh
# Emit TAP for dither class-id annotation and gperf registry sync checks.

set -eu

src_root=$1
generator=$src_root/tools/gen_dither_classid_gperf.awk
gperf_file=$src_root/src/classid-dither.gperf

echo "1..1"

if test ! -f "$generator"; then
    echo "not ok 1 - dither classid registry stays in sync"
    echo "# missing generator: $generator"
    exit 1
fi

if test ! -f "$gperf_file"; then
    echo "not ok 1 - dither classid registry stays in sync"
    echo "# missing gperf file: $gperf_file"
    exit 1
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-dither-classid-sync-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

expected=$tmpdir/classid-dither.expected.gperf

if ! awk -f "$generator" \
    "$src_root"/src/dither-policy-none.h \
    "$src_root"/src/dither-policy-fs.h \
    "$src_root"/src/dither-policy-atkinson.h \
    "$src_root"/src/dither-policy-jajuni.h \
    "$src_root"/src/dither-policy-stucki.h \
    "$src_root"/src/dither-policy-burkes.h \
    "$src_root"/src/dither-policy-sierra1.h \
    "$src_root"/src/dither-policy-sierra2.h \
    "$src_root"/src/dither-policy-sierra3.h \
    "$src_root"/src/dither-policy-lso2.h \
    "$src_root"/src/dither-policy-a-dither.h \
    "$src_root"/src/dither-policy-x-dither.h \
    "$src_root"/src/dither-policy-bluenoise.h \
    "$src_root"/src/dither-policy-interframe.h \
    >"$expected"; then
    echo "not ok 1 - dither classid registry stays in sync"
    echo "# failed to regenerate classid-dither.gperf"
    exit 1
fi

if cmp -s "$gperf_file" "$expected"; then
    echo "ok 1 - dither classid registry stays in sync"
    exit 0
fi

echo "not ok 1 - dither classid registry stays in sync"
if command -v diff >/dev/null 2>&1; then
    diff -u "$gperf_file" "$expected" | sed 's/^/# /'
else
    echo "# diff not found"
fi
exit 1
