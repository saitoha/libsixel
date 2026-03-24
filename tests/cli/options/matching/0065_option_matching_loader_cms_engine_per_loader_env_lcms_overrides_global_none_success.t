#!/bin/sh
# TAP test verifying per-loader env alias lcms overrides global none.

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
output_ref_cms1="${ARTIFACT_LOCAL_DIR}/cms_per_loader_lcms_alias_ref_cms1.six"
output_override_lcms="${ARTIFACT_LOCAL_DIR}/cms_per_loader_lcms_alias_actual.six"

run_img2sixel -Llibwebp:cms=1! "${input_webp}" >"${output_ref_cms1}" || {
    echo "not ok" 1 - "cms=1 reference decode failed"
    exit 0
}

run_img2sixel \
    --env "SIXEL_LOADER_CMS_ENGINE=none" \
    --env "SIXEL_LOADER_LIBWEBP_CMS_ENGINE=lcms" \
    -Llibwebp! "${input_webp}" >"${output_override_lcms}" || {
    echo "not ok" 1 - "per-loader lcms alias decode failed"
    exit 0
}

cmp -s "${output_ref_cms1}" "${output_override_lcms}" || {
    echo "not ok" 1 - "SIXEL_LOADER_LIBWEBP_CMS_ENGINE=lcms did not override global none"
    exit 0
}

echo "ok" 1 - "SIXEL_LOADER_LIBWEBP_CMS_ENGINE=lcms overrides global none"
exit 0
