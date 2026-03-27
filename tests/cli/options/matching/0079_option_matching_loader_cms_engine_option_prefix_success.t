#!/bin/sh
# TAP test verifying --cms-engine and -# accept unique prefixes.

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
output_ref="${ARTIFACT_LOCAL_DIR}/cms_engine_prefix_ref_auto.six"
output_long_prefix="${ARTIFACT_LOCAL_DIR}/cms_engine_prefix_long_au.six"
output_short_prefix="${ARTIFACT_LOCAL_DIR}/cms_engine_prefix_short_au.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --cms-engine=auto \
    -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_ref}" || {
    echo "not ok" 1 - "--cms-engine=auto reference decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --cms-engine=au \
    -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_long_prefix}" || {
    echo "not ok" 1 - "--cms-engine=au prefix was rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -# au \
    -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_short_prefix}" || {
    echo "not ok" 1 - "-# au prefix was rejected"
    exit 0
}

cmp -s "${output_ref}" "${output_long_prefix}" || {
    echo "not ok" 1 - "--cms-engine=au diverged from --cms-engine=auto"
    exit 0
}

cmp -s "${output_ref}" "${output_short_prefix}" || {
    echo "not ok" 1 - "-# au diverged from --cms-engine=auto"
    exit 0
}

echo "ok" 1 - "--cms-engine and -# accept unique prefixes"
exit 0
