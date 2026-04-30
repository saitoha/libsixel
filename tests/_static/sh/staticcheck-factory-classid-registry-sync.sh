#!/bin/sh
# Emit TAP for unified factory class-id annotation and gperf sync checks.

set -eu

src_root=$1
generator=$src_root/tools/gen_factory_classid_gperf.awk
gperf_file=$src_root/src/classid-factory.gperf

echo "1..1"

if test ! -f "$generator"; then
    echo "not ok 1 - factory classid registry stays in sync"
    echo "# missing generator: $generator"
    exit 1
fi

if test ! -f "$gperf_file"; then
    echo "not ok 1 - factory classid registry stays in sync"
    echo "# missing gperf file: $gperf_file"
    exit 1
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-factory-classid-sync-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

expected=$tmpdir/classid-factory.expected.gperf

if ! awk -f "$generator" \
    "$src_root"/src/lookup-policy-5bit.h \
    "$src_root"/src/lookup-policy-6bit.h \
    "$src_root"/src/lookup-policy-certlut.h \
    "$src_root"/src/lookup-policy-eytzinger.h \
    "$src_root"/src/lookup-policy-fhedt.h \
    "$src_root"/src/lookup-policy-mahalanobis.h \
    "$src_root"/src/lookup-policy-mono-darkbg.h \
    "$src_root"/src/lookup-policy-mono-lightbg.h \
    "$src_root"/src/lookup-policy-none.h \
    "$src_root"/src/lookup-policy-rbc.h \
    "$src_root"/src/lookup-policy-vptree.h \
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
    "$src_root"/src/loader-builtin.h \
    "$src_root"/src/loader-coregraphics.h \
    "$src_root"/src/loader-gd.h \
    "$src_root"/src/loader-gdk-pixbuf2.h \
    "$src_root"/src/loader-gnome-thumbnailer.h \
    "$src_root"/src/loader-libjpeg.h \
    "$src_root"/src/loader-libpng.h \
    "$src_root"/src/loader-librsvg.h \
    "$src_root"/src/loader-libtiff.h \
    "$src_root"/src/loader-libwebp.h \
    "$src_root"/src/loader-quicklook.h \
    "$src_root"/src/loader-wic.h \
    "$src_root"/src/frame.h \
    "$src_root"/src/loader-manager.h \
    >"$expected"; then
    echo "not ok 1 - factory classid registry stays in sync"
    echo "# failed to regenerate classid-factory.gperf"
    exit 1
fi

if cmp -s "$gperf_file" "$expected"; then
    echo "ok 1 - factory classid registry stays in sync"
    exit 0
fi

echo "not ok 1 - factory classid registry stays in sync"
if command -v diff >/dev/null 2>&1; then
    diff -u "$gperf_file" "$expected" | sed 's/^/# /'
else
    echo "# diff not found"
fi
exit 1
