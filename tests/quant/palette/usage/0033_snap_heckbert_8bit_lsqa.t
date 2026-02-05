#!/bin/sh
# Run lsqa quality checks for Heckbert palette snapping with 8-bit palettes.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

lsqa_common_path="${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"
. "${lsqa_common_path}"

status=0

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

ensure_img2sixel_available

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/snap-heckbert-8bit.six"
output_png="${ARTIFACT_LOCAL_DIR}/snap-heckbert-8bit.png"

SIXEL_PALETTE_SNAP_TARGET_POLICY=reversible
SIXEL_PALETTE_SNAP_TIMING_POLICY=all
SIXEL_PALETTE_SNAP_APPROACH_RATE=0.7
SIXEL_PALETTE_SNAP_CHANNEL_FACTOR_L=0.7
export SIXEL_PALETTE_SNAP_TARGET_POLICY
export SIXEL_PALETTE_SNAP_TIMING_POLICY
export SIXEL_PALETTE_SNAP_APPROACH_RATE
export SIXEL_PALETTE_SNAP_CHANNEL_FACTOR_L

if run_img2sixel -Q heckbert -6 -o "${output_sixel}" "${input_image}"; then
    :
else
    fail 1 "img2sixel snap heckbert 8bit failed"
    exit "${status}"
fi

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "snap heckbert 8bit lsqa passed"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "snap heckbert 8bit lsqa failed"
fi

exit "${status}"
