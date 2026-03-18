#!/bin/sh
# Verify palette PNG quality respects the lsqa baseline and relaxed floor.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

lsqa_floor=0.99
image_path="${TOP_SRCDIR}/tests/data/inputs/formats/palette.png"
expected_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/0004_palette_png_default_black_composite.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/palette.six"
run_img2sixel -Lbuiltin! "${image_path}" >"${output_sixel}" || {
    echo "not ok" 1 - "palette quality regressed"
    exit 0
}

lsqa_err=$(
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${expected_ppm}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

lsqa_status=${lsqa_run_status-0}

test "${lsqa_status}" -ne 5 || {
    echo "not ok" 1 - "${lsqa_err}"
    exit 0
}

test "${lsqa_status}" -eq 0 || {
    echo "not ok" 1 - "palette quality regressed"
    exit 0
}

echo "ok" 1 - "palette quality meets baseline"
exit 0
