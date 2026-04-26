#!/bin/sh
# Emit TAP for dither backend-dispatch remnants in policy/hot-loop paths.

set -eu

echo "1..1"

src_root=$1
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-dither-dispatch-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

bad=$tmpdir/bad.txt
: > "$bad"

for path in \
    "$src_root/src/dither-policy-backend.c" \
    "$src_root/src/dither-policy-backend.h" \
    "$src_root/src/dither-fixed-8bit.c" \
    "$src_root/src/dither-fixed-8bit.h" \
    "$src_root/src/dither-fixed-float32.c" \
    "$src_root/src/dither-fixed-float32.h" \
    "$src_root/src/dither-positional-8bit.c" \
    "$src_root/src/dither-positional-8bit.h" \
    "$src_root/src/dither-positional-float32.c" \
    "$src_root/src/dither-positional-float32.h" \
    "$src_root/src/dither-varcoeff-8bit.c" \
    "$src_root/src/dither-varcoeff-8bit.h" \
    "$src_root/src/dither-varcoeff-float32.c" \
    "$src_root/src/dither-varcoeff-float32.h"
do
    test -f "$path" || continue
    printf "%s:1:obsolete dither policy backend file must not exist\n" \
        "$path" >> "$bad"
done

for path in \
    "$src_root/src/Makefile.am" \
    "$src_root/src/Makefile.in" \
    "$src_root/src/meson.build" \
    "$src_root/amalgamation/meson.build"
do
    test -f "$path" || continue
    awk '
    /dither-policy-backend/ {
        printf "%s:%d:%s\n", FILENAME, NR, $0
    }
    ' "$path"
done >> "$bad"

find "$src_root/src" -maxdepth 1 -type f -name 'dither-policy-*.c' \
    -print | LC_ALL=C sort | while IFS= read -r path
do
    awk '
    /#include "dither-fixed-8bit.h"/ {
        printf "%s:%d:%s\n", FILENAME, NR, $0
    }
    /#include "dither-fixed-float32.h"/ {
        printf "%s:%d:%s\n", FILENAME, NR, $0
    }
    /#include "dither-positional-8bit.h"/ {
        printf "%s:%d:%s\n", FILENAME, NR, $0
    }
    /#include "dither-positional-float32.h"/ {
        printf "%s:%d:%s\n", FILENAME, NR, $0
    }
    /#include "dither-varcoeff-8bit.h"/ {
        printf "%s:%d:%s\n", FILENAME, NR, $0
    }
    /#include "dither-varcoeff-float32.h"/ {
        printf "%s:%d:%s\n", FILENAME, NR, $0
    }
    /#include "dither-policy-backend.h"/ {
        printf "%s:%d:%s\n", FILENAME, NR, $0
    }
    /sixel_dither_policy_backend_apply_(fixed|varcoeff|positional)/ {
        printf "%s:%d:%s\n", FILENAME, NR, $0
    }
    /context[[:space:]]*\.[[:space:]]*method_for_diffuse[[:space:]]*=/ {
        printf "%s:%d:%s\n", FILENAME, NR, $0
    }
    /sixel_dither_apply_(fixed|positional|varcoeff)_(8bit|float32)[[:space:]]*\(/ {
        printf "%s:%d:%s\n", FILENAME, NR, $0
    }
    ' "$path"
done >> "$bad"

for path in \
    "$src_root/src/dither-policy-fixed-8bit.inc.h" \
    "$src_root/src/dither-policy-fixed-float32.inc.h" \
    "$src_root/src/dither-policy-varcoeff-8bit.inc.h" \
    "$src_root/src/dither-policy-varcoeff-float32.inc.h" \
    "$src_root/src/dither-policy-positional-8bit.inc.h" \
    "$src_root/src/dither-policy-positional-float32.inc.h"
do
    test -f "$path" || continue
    awk '
    /(^|[^A-Za-z0-9_])(f_diffuse|f_mask|varerr_diffuse)([^A-Za-z0-9_]|$)/ {
        printf "%s:%d:%s\n", FILENAME, NR, $0
    }
    ' "$path"
done >> "$bad"

if test -f "$src_root/src/dither-internal.h"; then
    awk '
    /(^|[^A-Za-z0-9_])method_for_diffuse([^A-Za-z0-9_]|$)/ {
        printf "%s:%d:%s\n", FILENAME, NR, $0
    }
    ' "$src_root/src/dither-internal.h" >> "$bad"
fi

if test -s "$bad"; then
    echo "not ok 1 - dither policies avoid backend dispatch remnants"
    sed 's/^/# /' "$bad"
    exit 1
fi

echo "ok 1 - dither policies avoid backend dispatch remnants"
