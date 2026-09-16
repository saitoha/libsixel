#!/bin/sh
# TAP test verifying repeated --cms-engine options use last occurrence.

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
output_ref_auto="${ARTIFACT_LOCAL_DIR}/cms_engine_repeated_ref_auto.six"
output_ref_none="${ARTIFACT_LOCAL_DIR}/cms_engine_repeated_ref_none.six"
output_last_auto="${ARTIFACT_LOCAL_DIR}/cms_engine_repeated_last_auto.six"
output_last_none="${ARTIFACT_LOCAL_DIR}/cms_engine_repeated_last_none.six"

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
    --cms-engine=auto \
    -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_last_auto}" || {
    echo "not ok" 1 - "repeated option (last auto) decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --cms-engine=auto \
    --cms-engine=none \
    -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_last_none}" || {
    echo "not ok" 1 - "repeated option (last none) decode failed"
    exit 0
}

cmp -s "${output_ref_auto}" "${output_last_auto}" || {
    echo "not ok" 1 - "last --cms-engine=auto did not take effect"
    exit 0
}

cmp -s "${output_ref_none}" "${output_last_none}" || {
    echo "not ok" 1 - "last --cms-engine=none did not take effect"
    exit 0
}

echo "ok" 1 - "repeated --cms-engine follows last occurrence"
exit 0
