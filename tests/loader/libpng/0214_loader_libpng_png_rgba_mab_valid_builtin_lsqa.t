#!/bin/sh
# Verify libpng builtin CMS path applies RGBA PNG embedded mAB ICC.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}
input_png="${TOP_SRCDIR}/tests/data/colormgmt/input/custom/rgba_mab_valid.png"
reference_six="${TOP_SRCDIR}/tests/data/colormgmt/reference/custom/rgba_mab_valid_png_builtin.six"
output_six="${TMPDIR:-/tmp}/libsixel-rgba-mab-valid-png-$$.six"

test -f "${input_png}" || {
    echo "not ok" 1 - "missing input fixture: rgba_mab_valid.png"
    exit 0
}

test -f "${reference_six}" || {
    echo "not ok" 1 - "missing reference fixture: rgba_mab_valid_png_builtin.six"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibpng:cms_engine=builtin! "${input_png}" >"${output_six}" || {
    echo "not ok" 1 - "libpng builtin cms decode failed: rgba_mab_valid.png"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${reference_six}" "${output_six}" 2>&1) || {
    echo "not ok" 1 - "rgba_mab_valid.png builtin cms fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "libpng builtin cms rgba_mab_valid keeps MS-SSIM ${lsqa_floor}"
exit 0
