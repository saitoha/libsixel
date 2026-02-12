#!/bin/sh
# Exercise the float32 VPTE lookup policy through img2sixel options.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/vpte-float32.six"

run_img2sixel --lookup-policy=vpte --precision=float32 -p 16 -d none \
        -o "${output_sixel}" "${snake_png}" || {
    fail 1 "float32 VPTE lookup policy failed"
    exit 0
}

pass 1 "float32 VPTE lookup policy completes"

exit 0
