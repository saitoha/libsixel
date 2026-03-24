#!/bin/sh
# TAP test verifying per-loader env alias color-sync maps to colorsync.

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
output_ref_none="${ARTIFACT_LOCAL_DIR}/cms_engine_color_sync_alias_ref_none.six"
output_colorsync="${ARTIFACT_LOCAL_DIR}/cms_engine_color_sync_alias_colorsync.six"
output_color_sync="${ARTIFACT_LOCAL_DIR}/cms_engine_color_sync_alias_color_sync.six"

run_img2sixel -Llibwebp:cms=0! "${input_webp}" >"${output_ref_none}" || {
    echo "not ok" 1 - "cms=0 reference decode failed"
    exit 0
}

run_img2sixel \
    --env "SIXEL_LOADER_CMS_ENGINE=none" \
    --env "SIXEL_LOADER_LIBWEBP_CMS_ENGINE=colorsync" \
    -Llibwebp! "${input_webp}" >"${output_colorsync}" || {
    echo "not ok" 1 - "SIXEL_LOADER_LIBWEBP_CMS_ENGINE=colorsync decode failed"
    exit 0
}

run_img2sixel \
    --env "SIXEL_LOADER_CMS_ENGINE=none" \
    --env "SIXEL_LOADER_LIBWEBP_CMS_ENGINE=color-sync" \
    -Llibwebp! "${input_webp}" >"${output_color_sync}" || {
    echo "not ok" 1 - "SIXEL_LOADER_LIBWEBP_CMS_ENGINE=color-sync decode failed"
    exit 0
}

lsqa_status=0
lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.995" \
    "${output_colorsync}" "${output_ref_none}" 2>&1) || lsqa_status=$?
test "${lsqa_status}" -eq 5 || {
    echo "not ok" 1 - "colorsync env did not differ from none baseline: ${lsqa_msg-}"
    exit 0
}

cmp -s "${output_colorsync}" "${output_color_sync}" || {
    echo "not ok" 1 - "color-sync alias did not match colorsync behavior"
    exit 0
}

echo "ok" 1 - "per-loader env color-sync alias maps to colorsync"
exit 0
