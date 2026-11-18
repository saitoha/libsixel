#!/bin/sh
# Run from the current source root with the r2 checkout and a build directory.
set -eu

baseline=$(cd "$1" && pwd -P)
current=$(pwd -P)
export PATH="$current/.local/bin:$PATH"
mkdir -p "$2"
build=$(cd "$2" && pwd -P)
jobs=${JOBS:-$(nproc)}

# Only the release header and shared library are needed for the old client.
# Use the pre-C23 C dialect expected by the release even on recent GCC.
mkdir -p "$build/legacy" "$build/current"
cd "$build/legacy"
"$baseline/configure" --disable-static --without-jpeg --without-png \
    CFLAGS="-O2 -std=gnu11"
make -j"$jobs" -C src

cd "$build/current"
"$current/configure" --enable-shared --without-lcms2 --without-coregraphics \
    --with-png
grep -qx '#define HAVE_LIBPNG 1' config.h
if grep -Eq '^#define HAVE_(LCMS2|COREGRAPHICS) 1$' config.h; then
    echo 'The builtin CMS compatibility job requires external CMS backends to be disabled.' >&2
    exit 1
fi
make -j"$jobs"
sh "$current/tests/compat/v1.8.7-r2/check.sh" \
    "$build/legacy/include/sixel.h" "$build/legacy/src/.libs/libsixel.so" \
    "$build/current/src/.libs/libsixel.so" "$build/current/include/sixel.h"

# Name each mandatory observation so deleting or skipping one fails this job.
set -- \
    cli/terminal-color/0040_save_0.t \
    cli/terminal-color/0044_srgb.t \
    cli/terminal-color/0145_srgb_wire.t \
    color/bpc/0022_output_bpc_builtin_float.t \
    color/bpc/0023_output_bpc_libpng_float.t \
    color/bpc/0024_output_bpc_builtin_sixel.t \
    color/bpc/0025_output_bpc_libpng_sixel.t \
    color/bpc/0026_output_bpc_conflict.t \
    color/bpc/0029_output_bpc_auto_tty.t \
    color/bpc/0030_output_bpc_builtin_off_registers.t \
    color/bpc/0031_output_bpc_libpng_off_registers.t \
    color/bpc/0032_output_bpc_builtin_on_registers.t \
    color/bpc/0033_output_bpc_libpng_on_registers.t \
    color/bpc/0034_output_bpc_builtin_off_indices.t \
    color/bpc/0035_output_bpc_libpng_off_indices.t \
    color/bpc/0036_output_bpc_builtin_on_indices.t \
    color/bpc/0037_output_bpc_libpng_on_indices.t \
    color/bpc/0038_output_bpc_builtin_off_float.t \
    color/bpc/0039_output_bpc_libpng_off_float.t \
    palette/snap-destination/0003_p3_unsnapped_palette_lookup.t
make -j"$jobs" -C tests check TESTS="$*"
for test_name do
    awk '
        $1 == ":test-result:" { observations++; if ($2 != "PASS") bad = 1 }
        $1 == ":global-test-result:" { summaries++; if ($2 != "PASS") bad = 1 }
        END {
            if (observations == 0 || summaries != 1 || bad) {
                print "Required test did not pass: " FILENAME
                exit 1
            }
        }
    ' "tests/${test_name%.t}.trs"
done
