#!/bin/sh
# Verify builtin PIC decode keeps MS-SSIM quality for snake16 fixture.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test -x "${LSQA_PATH-}" || {
    printf "1..0 # SKIP lsqa is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_pic="${TOP_SRCDIR}/tests/data/inputs/formats/pic_valid_snake16_raw_rgb.pic"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/pic_snake16_reference.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/builtin_pic_snake16_quality.six"
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_pic}" >"${output_sixel}" || {
    echo "not ok" 1 - "builtin PIC snake16 decode failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:${lsqa_floor}"     "${reference_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "builtin PIC snake16 fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "builtin PIC snake16 keeps MS-SSIM ${lsqa_floor}"
exit 0
