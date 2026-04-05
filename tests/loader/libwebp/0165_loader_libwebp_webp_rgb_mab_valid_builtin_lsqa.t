#!/bin/sh
# Verify libwebp builtin CMS path renders embedded RGB mAB ICC profile.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}
input_webp="${TOP_SRCDIR}/tests/data/colormgmt/input/custom/rgb_mab_valid.webp"
reference_six="${TOP_SRCDIR}/tests/data/colormgmt/reference/custom/rgb_mab_valid_webp_builtin.six"
output_six="${TMPDIR:-/tmp}/libsixel-rgb-mab-valid-webp-$$.six"

test -f "${input_webp}" || {
    echo "not ok" 1 - "missing input fixture: rgb_mab_valid.webp"
    exit 0
}

test -f "${reference_six}" || {
    echo "not ok" 1 - "missing reference fixture: rgb_mab_valid_webp_builtin.six"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=builtin! "${input_webp}" >"${output_six}" || {
    echo "not ok" 1 - "libwebp builtin cms decode failed: rgb_mab_valid.webp"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${reference_six}" "${output_six}" 2>&1) || {
    echo "not ok" 1 - "rgb_mab_valid.webp builtin cms fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "libwebp builtin cms rgb_mab_valid keeps MS-SSIM ${lsqa_floor}"
exit 0
