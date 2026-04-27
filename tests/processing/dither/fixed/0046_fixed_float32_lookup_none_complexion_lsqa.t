#!/bin/sh
# TAP test covering fixed float32 dither with lookup none and deprecated -C.
#
# Flow:
# - Convert the snake reference with float32 precision and no LUT lookup.
# - Pass a non-default complexion score to verify the deprecated no-op path.
# - Validate quality with lsqa to keep output stability.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

# Threshold rationale:
# - lookup-policy=none disables LUT shortcuts and keeps this test focused on
#   float32 nearest-color scanning behavior.
# - fixed/fs with lookup-policy=none has moderate variance across platforms, so
#   0.94 keeps false negatives low while still catching visible regressions.
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.94}


input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/fixed-float32-lookup-none-complexion.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs --precision=float32 --lookup-policy=none \
        -C 8 -p 16 -o "${output_sixel}" "${input_image}" || {
    echo "not ok" 1 - "fixed float32 lookup none deprecated option conversion failed"
    exit 0
}

lsqa_err=$(
    set +xv
    ${SIXEL_RUNTIME-} "${LSQA_PATH}" -b "MS-SSIM:${lsqa_floor}" "${input_image}" \
        "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    echo "ok" 1 - "fixed float32 lookup none deprecated option lsqa passed"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    echo "not ok" 1 - "${lsqa_err}"
    exit 0
}

echo "not ok" 1 - "fixed float32 lookup none deprecated option lsqa failed"

exit 0
