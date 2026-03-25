#!/bin/sh
# TAP test verifying option/env order decides global cms engine precedence.

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
output_ref_auto="${ARTIFACT_LOCAL_DIR}/cms_engine_precedence_ref_auto.six"
output_ref_none="${ARTIFACT_LOCAL_DIR}/cms_engine_precedence_ref_none.six"
output_option_last="${ARTIFACT_LOCAL_DIR}/cms_engine_precedence_option_last.six"
output_env_last="${ARTIFACT_LOCAL_DIR}/cms_engine_precedence_env_last.six"

run_img2sixel \
    --env "SIXEL_LOADER_CMS_ENGINE=auto" \
    -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_ref_auto}" || {
    echo "not ok" 1 - "auto reference decode failed"
    exit 0
}

run_img2sixel \
    --env "SIXEL_LOADER_CMS_ENGINE=none" \
    -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_ref_none}" || {
    echo "not ok" 1 - "none reference decode failed"
    exit 0
}

run_img2sixel \
    --env "SIXEL_LOADER_CMS_ENGINE=none" \
    -# auto \
    -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_option_last}" || {
    echo "not ok" 1 - "option-last precedence decode failed"
    exit 0
}

run_img2sixel \
    -# auto \
    --env "SIXEL_LOADER_CMS_ENGINE=none" \
    -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_env_last}" || {
    echo "not ok" 1 - "env-last precedence decode failed"
    exit 0
}

cmp -s "${output_ref_auto}" "${output_option_last}" || {
    echo "not ok" 1 - "option-last precedence did not match auto reference"
    exit 0
}

cmp -s "${output_ref_none}" "${output_env_last}" || {
    echo "not ok" 1 - "env-last precedence did not match none reference"
    exit 0
}

echo "ok" 1 - "global cms engine precedence follows option/env order"
exit 0
