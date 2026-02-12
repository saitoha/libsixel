#!/bin/sh
# Exercise the 8-bit VP-tree lookup policy through img2sixel options.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/vptree-8bit.six"

run_img2sixel --lookup-policy=vptree -p 16 -d none \
        -o "${output_sixel}" "${snake_png}" || {
    fail 1 "8-bit VP-tree lookup policy failed"
    exit 0
}

pass 1 "8-bit VP-tree lookup policy completes"

exit 0
