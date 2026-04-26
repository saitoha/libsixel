#!/bin/sh
# Emit TAP for dither policy warning/analyzer compile checks.

set -eu

src_root=${1:-}
build_root=${2:-${TOP_BUILDDIR:-$src_root}}
cc_bin=${3:-${CC:-cc}}

echo "1..1"

if test -z "$src_root"; then
    echo "not ok 1 - dither policy warning compile check"
    echo "# src_root argument is required"
    exit 1
fi

if test ! -f "$build_root/config.h"; then
    echo "ok 1 # SKIP missing config.h under $build_root"
    exit 0
fi

if ! command -v "$cc_bin" >/dev/null 2>&1; then
    echo "ok 1 # SKIP compiler not found: $cc_bin"
    exit 0
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-dither-policy-compile-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

log=$tmpdir/compile.log
failed=0

for source in \
    dither-policy-a-dither.c \
    dither-policy-x-dither.c \
    dither-policy-none.c \
    dither-policy-fs.c
do
    if ! "$cc_bin" \
            -DHAVE_CONFIG_H \
            -D_POSIX_C_SOURCE=200809L \
            -DSIXEL_ENABLE_THREADS=1 \
            -I"$build_root" \
            -I"$build_root/include" \
            -I"$src_root" \
            -I"$src_root/src" \
            -I"$src_root/include" \
            -std=c99 \
            -Wall \
            -Wextra \
            -Wformat=2 \
            -Werror \
            -c "$src_root/src/$source" \
            -o "$tmpdir/${source%.c}.strict.o" \
            >> "$log" 2>&1; then
        failed=1
    fi
done

if "$cc_bin" -x c -c /dev/null -o "$tmpdir/empty.o" -fanalyzer \
        >/dev/null 2>&1; then
    for source in dither-policy-none.c dither-policy-fs.c
    do
        if ! "$cc_bin" \
                -DHAVE_CONFIG_H \
                -D_POSIX_C_SOURCE=200809L \
                -DSIXEL_ENABLE_THREADS=1 \
                -I"$build_root" \
                -I"$build_root/include" \
                -I"$src_root" \
                -I"$src_root/src" \
                -I"$src_root/include" \
                -std=c99 \
                -O2 \
                -fanalyzer \
                -Wall \
                -Wextra \
                -Wformat=2 \
                -Werror \
                -c "$src_root/src/$source" \
                -o "$tmpdir/${source%.c}.analyzer.o" \
                >> "$log" 2>&1; then
            failed=1
        fi
    done
else
    printf '# analyzer compile skipped: %s lacks -fanalyzer\n' \
        "$cc_bin" >> "$log"
fi

if test "$failed" -ne 0; then
    echo "not ok 1 - dither policy warning compile check"
    sed 's/^/# /' "$log"
    exit 1
fi

echo "ok 1 - dither policy warning compile check"
exit 0

