#!/bin/sh
# Verify libjpeg builtin CMS path renders embedded RGB mAB ICC profile.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_JPEG-}" = 1 || {
    printf "1..0 # SKIP libjpeg loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}
input_jpeg="${TOP_SRCDIR}/tests/data/colormgmt/input/custom/rgb_mab_valid.jpg"
reference_six="${TOP_SRCDIR}/tests/data/colormgmt/reference/custom/rgb_mab_valid_jpeg_builtin.six"
output_six="${TMPDIR:-/tmp}/libsixel-rgb-mab-valid-jpeg-$$.six"

test -f "${input_jpeg}" || {
    echo "not ok" 1 - "missing input fixture: rgb_mab_valid.jpg"
    exit 0
}

test -f "${reference_six}" || {
    echo "not ok" 1 - "missing reference fixture: rgb_mab_valid_jpeg_builtin.six"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibjpeg:cms_engine=builtin! "${input_jpeg}" >"${output_six}" || {
    echo "not ok" 1 - "libjpeg builtin cms decode failed: rgb_mab_valid.jpg"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${reference_six}" "${output_six}" 2>&1) || {
    echo "not ok" 1 - "rgb_mab_valid.jpg builtin cms fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "libjpeg builtin cms rgb_mab_valid keeps MS-SSIM ${lsqa_floor}"
exit 0
