#!/bin/sh
# Verify WIC JPEG grayscale decoding quality with an MS-SSIM baseline.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}
test "${HAVE_WIC-}" = 1 || {
    printf "1..0 # SKIP wic loader is unavailable\n";
    exit 0
}
test "${RUNTIME_ENV_IS_WINE-0}" -eq 1 && {
    printf "1..0 # SKIP WIC is unavailable under wine\n";
    exit 0
}


printf '1..1\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

lsqa_floor=0.96

image_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-grayscale.jpg"
reference_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-gray.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/wic_jpeg_grayscale.six"
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lwic! "${image_path}" >"${output_sixel}" || {
    echo "not ok" 1 - "wic jpeg grayscale conversion failed"
    exit 0
}

lsqa_err=$(
    set +xv
    ${SIXEL_RUNTIME-} "${LSQA_PATH}" -b "MS-SSIM:${lsqa_floor}" "${reference_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    echo "ok" 1 - "wic jpeg grayscale quality meets baseline"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    echo "not ok" 1 - "${lsqa_err}"
    exit 0
}

echo "not ok" 1 - "wic jpeg grayscale quality regressed"

exit 0
