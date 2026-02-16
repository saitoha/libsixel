#!/bin/sh
# TAP test covering varcoeff float32 lookup none with raster scan.
#
# Flow:
# - Convert the snake reference with lso2, float32 precision, and no LUT.
# - Force raster scan to complement existing serpentine coverage.
# - Validate quality with lsqa to keep output quality bounded.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

# Threshold rationale:
# - lookup-policy=none forces the float32 full scan path that this case wants
#   to validate.
# - lso2 varcoeff diffusion is more stable than fs under the same palette size,
#   so 0.95 is strict enough to catch regressions without becoming flaky.
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.95}

echo "1..1"
set -v

input_image="${top_srcdir}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/varcoeff-float32-lookup-none-raster.six"

run_img2sixel -d lso2 -y raster --precision=float32 \
        --lookup-policy=none -p 16 -o "${output_sixel}" "${input_image}" || {
    fail 1 "varcoeff float32 lookup none raster conversion failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}" \
        "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    pass 1 "varcoeff float32 lookup none raster lsqa passed"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    fail 1 "${lsqa_err}"
    exit 0
}

fail 1 "varcoeff float32 lookup none raster lsqa failed"

exit 0
