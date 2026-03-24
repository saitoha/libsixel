#!/bin/sh
# TAP test verifying invalid per-loader cms-engine env falls back to global.

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
output_ref_cms1="${ARTIFACT_LOCAL_DIR}/cms_engine_env_invalid_ref_cms1.six"
output_invalid_per_loader="${ARTIFACT_LOCAL_DIR}/cms_engine_env_invalid_per_loader.six"

run_img2sixel -Llibwebp:cms=1! "${input_webp}" >"${output_ref_cms1}" || {
    echo "not ok" 1 - "cms=1 reference decode failed"
    exit 0
}

run_img2sixel \
    --env "SIXEL_LOADER_CMS_ENGINE=auto" \
    --env "SIXEL_LOADER_LIBWEBP_CMS_ENGINE=invalid-token" \
    -Llibwebp! "${input_webp}" >"${output_invalid_per_loader}" || {
    echo "not ok" 1 - "invalid per-loader cms engine env decode failed"
    exit 0
}

cmp -s "${output_ref_cms1}" "${output_invalid_per_loader}" || {
    echo "not ok" 1 - "invalid per-loader env did not fallback to global cms engine"
    exit 0
}
echo "ok" 1 - "invalid per-loader env fallback follows global value"

exit 0
