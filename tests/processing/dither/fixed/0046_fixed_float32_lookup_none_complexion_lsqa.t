#!/bin/sh
# TAP test covering fixed float32 dither with lookup none and complexion.
#
# Flow:
# - Convert the snake reference with float32 precision and no LUT lookup.
# - Apply a non-default complexion score to exercise weighted distance.
# - Validate quality with lsqa to keep output stability.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

# Threshold rationale:
# - lookup-policy=none disables LUT shortcuts and keeps this test focused on
#   float32 nearest-color scanning behavior.
# - fixed/fs plus complexion weighting causes slightly larger variance than the
#   default gamma path, so 0.94 keeps false negatives low across platforms
#   while still catching visible regressions.
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.94}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/fixed-float32-lookup-none-complexion.six"

run_img2sixel -d fs --precision=float32 --lookup-policy=none \
        -C 8 -p 16 -o "${output_sixel}" "${input_image}" || {
    fail 1 "fixed float32 lookup none complexion conversion failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}" \
        "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    pass 1 "fixed float32 lookup none complexion lsqa passed"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    fail 1 "${lsqa_err}"
    exit 0
}

fail 1 "fixed float32 lookup none complexion lsqa failed"

exit 0
