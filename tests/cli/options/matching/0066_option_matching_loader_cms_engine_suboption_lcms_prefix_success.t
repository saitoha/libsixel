#!/bin/sh
# TAP test verifying loader cms_engine suboption accepts lcms as lcms2 prefix.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_webp="${TOP_SRCDIR}/tests/data/inputs/snake_64_embedded_a98_icc.webp"
output_lcms2="${ARTIFACT_LOCAL_DIR}/cms_subopt_lcms_prefix_ref_lcms2.six"
output_lcms="${ARTIFACT_LOCAL_DIR}/cms_subopt_lcms_prefix_actual_lcms.six"

run_img2sixel -Llibwebp:cms=0:cms_engine=lcms2! "${input_webp}" >"${output_lcms2}" || {
    echo "not ok" 1 - "cms_engine=lcms2 reference decode failed"
    exit 0
}

run_img2sixel -Llibwebp:cms=0:cms_engine=lcms! "${input_webp}" >"${output_lcms}" || {
    echo "not ok" 1 - "cms_engine=lcms prefix decode failed"
    exit 0
}

cmp -s "${output_lcms2}" "${output_lcms}" || {
    echo "not ok" 1 - "cms_engine=lcms did not resolve as lcms2 prefix"
    exit 0
}

echo "ok" 1 - "cms_engine=lcms resolves as lcms2 prefix"
exit 0
