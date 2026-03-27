#!/bin/sh
# TAP test verifying invalid global cms-engine env falls back safely.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n";
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_webp="${TOP_SRCDIR}/tests/data/inputs/snake_64_embedded_a98_icc.webp"
output_ref_cms0="${ARTIFACT_LOCAL_DIR}/cms_engine_env_invalid_ref_cms0.six"
output_invalid_global="${ARTIFACT_LOCAL_DIR}/cms_engine_env_invalid_global.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! "${input_webp}" >"${output_ref_cms0}" || {
    echo "not ok" 1 - "cms=0 reference decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_CMS_ENGINE=invalid-token" \
    -Llibwebp! "${input_webp}" >"${output_invalid_global}" || {
    echo "not ok" 1 - "invalid global cms engine env decode failed"
    exit 0
}

cmp -s "${output_ref_cms0}" "${output_invalid_global}" || {
    echo "not ok" 1 - "invalid SIXEL_LOADER_CMS_ENGINE did not fallback to default none"
    exit 0
}
echo "ok" 1 - "invalid global cms engine env falls back to default"

exit 0
