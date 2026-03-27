#!/bin/sh
# TAP test verifying --cms-engine affects loaders without explicit
# :cms_engine suboptions.

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
output_ref_cms1="${ARTIFACT_LOCAL_DIR}/cms_engine_option_default_ref_cms1.six"
output_ref_cms0="${ARTIFACT_LOCAL_DIR}/cms_engine_option_default_ref_cms0.six"
output_option_auto="${ARTIFACT_LOCAL_DIR}/cms_engine_option_default_option_auto.six"
output_option_none="${ARTIFACT_LOCAL_DIR}/cms_engine_option_default_option_none.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_ref_cms1}" || {
    echo "not ok" 1 - "cms=1 reference decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! "${input_webp}" >"${output_ref_cms0}" || {
    echo "not ok" 1 - "cms=0 reference decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --cms-engine=auto \
    -Llibwebp! "${input_webp}" >"${output_option_auto}" || {
    echo "not ok" 1 - "--cms-engine=auto with implicit cms decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --cms-engine=none \
    -Llibwebp! "${input_webp}" >"${output_option_none}" || {
    echo "not ok" 1 - "--cms-engine=none with implicit cms decode failed"
    exit 0
}

cmp -s "${output_ref_cms1}" "${output_option_auto}" || {
    echo "not ok" 1 - "--cms-engine=auto did not enable implicit loader cms"
    exit 0
}

cmp -s "${output_ref_cms0}" "${output_option_none}" || {
    echo "not ok" 1 - "--cms-engine=none did not disable implicit loader cms"
    exit 0
}

echo "ok" 1 - "--cms-engine controls implicit loader cms defaults"
exit 0
