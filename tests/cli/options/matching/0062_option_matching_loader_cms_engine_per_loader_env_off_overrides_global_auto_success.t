#!/bin/sh
# TAP test verifying per-loader env alias off overrides global auto.

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
output_ref_cms0="${ARTIFACT_LOCAL_DIR}/cms_per_loader_alias_ref_cms0.six"
output_override_off="${ARTIFACT_LOCAL_DIR}/cms_per_loader_alias_override_off.six"

run_img2sixel -Llibwebp:cms_engine=none! "${input_webp}" >"${output_ref_cms0}" || {
    echo "not ok" 1 - "cms=0 reference decode failed"
    exit 0
}

run_img2sixel \
    --env "SIXEL_LOADER_CMS_ENGINE=auto" \
    --env "SIXEL_LOADER_LIBWEBP_CMS_ENGINE=off" \
    -Llibwebp! "${input_webp}" >"${output_override_off}" || {
    echo "not ok" 1 - "per-loader off alias decode failed"
    exit 0
}

cmp -s "${output_ref_cms0}" "${output_override_off}" || {
    echo "not ok" 1 - "SIXEL_LOADER_LIBWEBP_CMS_ENGINE=off did not override global auto"
    exit 0
}
echo "ok" 1 - "SIXEL_LOADER_LIBWEBP_CMS_ENGINE=off overrides global auto"

exit 0
