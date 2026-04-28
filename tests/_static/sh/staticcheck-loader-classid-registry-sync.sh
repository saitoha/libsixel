#!/bin/sh
# Emit TAP for loader class-id annotation and gperf registry sync checks.

set -eu

src_root=$1
generator=$src_root/tools/gen_loader_classid_gperf.awk
gperf_file=$src_root/src/classid-loader.gperf

echo "1..1"

if test ! -f "$generator"; then
    echo "not ok 1 - loader classid registry stays in sync"
    echo "# missing generator: $generator"
    exit 1
fi

if test ! -f "$gperf_file"; then
    echo "not ok 1 - loader classid registry stays in sync"
    echo "# missing gperf file: $gperf_file"
    exit 1
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-loader-classid-sync-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

expected=$tmpdir/classid-loader.expected.gperf

if ! awk -f "$generator" \
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
    >"$expected"; then
    echo "not ok 1 - loader classid registry stays in sync"
    echo "# failed to regenerate classid-loader.gperf"
    exit 1
fi

if cmp -s "$gperf_file" "$expected"; then
    echo "ok 1 - loader classid registry stays in sync"
    exit 0
fi

echo "not ok 1 - loader classid registry stays in sync"
if command -v diff >/dev/null 2>&1; then
    diff -u "$gperf_file" "$expected" | sed 's/^/# /'
else
    echo "# diff not found"
fi
exit 1
