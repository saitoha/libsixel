#!/bin/sh
# Run lsqa quality checks for Heckbert palette snapping with 8-bit palettes.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/snap-heckbert-8bit.six"

SIXEL_PALETTE_SNAP_TARGET_POLICY=reversible
SIXEL_PALETTE_SNAP_TIMING_POLICY=all
SIXEL_PALETTE_SNAP_APPROACH_RATE=0.7
SIXEL_PALETTE_SNAP_CHANNEL_FACTOR_L=0.7
export SIXEL_PALETTE_SNAP_TARGET_POLICY
export SIXEL_PALETTE_SNAP_TIMING_POLICY
export SIXEL_PALETTE_SNAP_APPROACH_RATE
export SIXEL_PALETTE_SNAP_CHANNEL_FACTOR_L

run_img2sixel -Q heckbert -6 -o "${output_sixel}" "${input_image}" || {
    fail 1 "img2sixel snap heckbert 8bit failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

[ "${lsqa_run_status:-0}" -eq 0 ] && {
    pass 1 "snap heckbert 8bit lsqa passed"
    exit 0
}

[ "${lsqa_run_status}" -eq 5 ] && {
    fail 1 "${lsqa_err}"
    exit 0
}

fail 1 "snap heckbert 8bit lsqa failed"

exit 0
