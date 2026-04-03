#!/bin/sh
# Verify builtin PSD mode7 CMYK8 path rejects truncated mft2 ICC and falls back.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}
input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_mode7_cmyk8_mft2_truncated_icc_profile.psd"
reference_six="${TOP_SRCDIR}/tests/data/colormgmt/reference/custom/cmyk_mft2_truncated_psd_builtin_fallback.six"
output_six="${TMPDIR:-/tmp}/libsixel-mode7-cmyk8-mft2-truncated-builtin-$$.six"

test -f "${input_psd}" || {
    echo "not ok" 1 - "missing input fixture: stbi_minimal_mode7_cmyk8_mft2_truncated_icc_profile.psd"
    exit 0
}

test -f "${reference_six}" || {
    echo "not ok" 1 - "missing reference fixture: cmyk_mft2_truncated_psd_builtin_fallback.six"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=builtin! "${input_psd}" >"${output_six}" || {
    echo "not ok" 1 - "builtin decode failed: stbi_minimal_mode7_cmyk8_mft2_truncated_icc_profile.psd"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${reference_six}" "${output_six}" 2>&1) || {
    echo "not ok" 1 - "mode7 cmyk8 truncated mft2 fallback fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "builtin PSD mode7 CMYK8 mft2 truncated fallback keeps MS-SSIM ${lsqa_floor}"
exit 0
