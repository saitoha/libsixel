#!/bin/sh
# TAP test covering varcoeff float32 lookup none with raster scan.
#
# Flow:
# - Convert the snake reference with lso2, float32 precision, and no LUT.
# - Force raster scan to complement existing serpentine coverage.
# - Validate quality with lsqa to keep output quality bounded.

# The reference five-scale MS-SSIM requires dimensions >=176x176.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

# The contract is float32 full palette scanning in the requested scan order.
# Use the default 256 colors; palette scarcity is not under test here.
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_192.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/varcoeff-float32-lookup-none-raster.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d lso2:scan=raster --precision=float32 \
        --lookup-policy=none -o "${output_sixel}" "${input_image}" || {
    echo "not ok" 1 - "varcoeff float32 lookup none raster conversion failed"
    exit 0
}

lsqa_err=$(
    set +xv
    ${SIXEL_RUNTIME-} "${LSQA_PATH}" -b "MS-SSIM:${lsqa_floor}" "${input_image}" \
        "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    echo "ok" 1 - "varcoeff float32 lookup none raster lsqa passed"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    echo "not ok" 1 - "${lsqa_err}"
    exit 0
}

echo "not ok" 1 - "varcoeff float32 lookup none raster lsqa failed"

exit 0
