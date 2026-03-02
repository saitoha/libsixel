#!/bin/sh
# Verify lcms2-enabled libpng decodes embedded ICC differently from builtin.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build\n"
    exit 0
}

test "${HAVE_LCMS2-}" = 1 || {
    printf "1..0 # SKIP lcms2 support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/map8_embedded_icc.png"
output_builtin="${ARTIFACT_LOCAL_DIR}/map8-builtin.sixel"
output_libpng="${ARTIFACT_LOCAL_DIR}/map8-libpng.sixel"

run_img2sixel -Lbuiltin! "${input_png}" >"${output_builtin}" || {
    echo "not ok" 1 "builtin decode failed"
    exit 0
}

run_img2sixel -Llibpng! "${input_png}" >"${output_libpng}" || {
    echo "not ok" 1 "libpng decode failed"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -b "MS-SSIM:0.999" "${output_builtin}" "${output_libpng}" 2>&1) || lsqa_status=$?

test "${lsqa_status-0}" -eq 5 || {
    echo "not ok" 1 "${lsqa_msg}"
    exit 0
}

echo "ok" 1 "lcms2 changes ICC decode output"
exit 0
