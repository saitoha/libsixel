#!/bin/sh
# Verify libtiff builtin CMS output for RGB parametric ICC fixture (types 3/4).

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_TIFF-}" = 1 || {
    printf "1..0 # SKIP libtiff loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}
input_tiff="${TOP_SRCDIR}/tests/data/colormgmt/input/custom/rgb_parametric_34.tiff"
reference_six="${TOP_SRCDIR}/tests/data/colormgmt/reference/custom/rgb_parametric_34_builtin.six"
output_six="${TMPDIR:-/tmp}/libsixel-rgb-parametric-34-$$.six"

test -f "${input_tiff}" || {
    echo "not ok" 1 - "missing input fixture: rgb_parametric_34.tiff"
    exit 0
}

test -f "${reference_six}" || {
    echo "not ok" 1 - "missing reference fixture: rgb_parametric_34_builtin.six"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibtiff:cms_engine=builtin! "${input_tiff}" >"${output_six}" || {
    echo "not ok" 1 - "libtiff builtin cms decode failed: rgb_parametric_34"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${reference_six}" "${output_six}" 2>&1) || {
    echo "not ok" 1 - "rgb_parametric_34 builtin cms fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "libtiff builtin cms rgb_parametric_34 keeps MS-SSIM ${lsqa_floor}"
exit 0
