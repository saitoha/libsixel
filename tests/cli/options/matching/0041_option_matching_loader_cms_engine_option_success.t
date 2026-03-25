#!/bin/sh
# TAP test verifying short -# maps to SIXEL_LOADER_CMS_ENGINE.

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
output_env="${ARTIFACT_LOCAL_DIR}/cms_engine_option_ref_env_auto.six"
output_short="${ARTIFACT_LOCAL_DIR}/cms_engine_option_short_auto.six"

run_img2sixel \
    --env "SIXEL_LOADER_CMS_ENGINE=auto" \
    -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_env}" || {
    echo "not ok" 1 - "reference decode with env cms engine failed"
    exit 0
}

run_img2sixel \
    -# auto \
    -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_short}" || {
    echo "not ok" 1 - "short -# option was rejected"
    exit 0
}

cmp -s "${output_env}" "${output_short}" || {
    echo "not ok" 1 - "short -# cms engine output diverged from env reference"
    exit 0
}
echo "ok" 1 - "short -# option mirrors SIXEL_LOADER_CMS_ENGINE"

exit 0
