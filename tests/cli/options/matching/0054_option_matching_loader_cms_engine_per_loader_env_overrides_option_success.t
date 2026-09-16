#!/bin/sh
# TAP test verifying per-loader CMS engine env overrides global option.

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
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/palette_lossless_embedded_a98_icc.webp"
output_ref_auto="${ARTIFACT_LOCAL_DIR}/cms_engine_env_override_option_ref_auto.six"
output_ref_none="${ARTIFACT_LOCAL_DIR}/cms_engine_env_override_option_ref_none.six"
output_env_auto="${ARTIFACT_LOCAL_DIR}/cms_engine_env_override_option_actual_auto.six"
output_env_none="${ARTIFACT_LOCAL_DIR}/cms_engine_env_override_option_actual_none.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --cms-engine=auto \
    -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_ref_auto}" || {
    echo "not ok" 1 - "auto reference decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --cms-engine=none \
    -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_ref_none}" || {
    echo "not ok" 1 - "none reference decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --cms-engine=none \
    --env "SIXEL_LOADER_LIBWEBP_CMS_ENGINE=auto" \
    -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_env_auto}" || {
    echo "not ok" 1 - "per-loader env auto override decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --cms-engine=auto \
    --env "SIXEL_LOADER_LIBWEBP_CMS_ENGINE=none" \
    -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_env_none}" || {
    echo "not ok" 1 - "per-loader env none override decode failed"
    exit 0
}

cmp -s "${output_ref_auto}" "${output_env_auto}" || {
    echo "not ok" 1 - "SIXEL_LOADER_LIBWEBP_CMS_ENGINE=auto did not override --cms-engine=none"
    exit 0
}

cmp -s "${output_ref_none}" "${output_env_none}" || {
    echo "not ok" 1 - "SIXEL_LOADER_LIBWEBP_CMS_ENGINE=none did not override --cms-engine=auto"
    exit 0
}

echo "ok" 1 - "per-loader CMS engine env overrides global --cms-engine"
exit 0
