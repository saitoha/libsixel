#!/bin/sh
# Verify libwebp default cms behavior matches explicit cms=0 for static ICC input.

set -eux

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_webp_icc="${TOP_SRCDIR}/tests/data/inputs/formats/palette_lossless_embedded_a98_icc.webp"
output_default="${ARTIFACT_LOCAL_DIR}/webp-icc-default-static.sixel"
output_cms0="${ARTIFACT_LOCAL_DIR}/webp-icc-cms0-static.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp! "${input_webp_icc}" >"${output_default}" || {
    echo "not ok" 1 - "libwebp default decode failed for ICC input (static)"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! "${input_webp_icc}" >"${output_cms0}" || {
    echo "not ok" 1 - "libwebp cms=0 decode failed for ICC input (static)"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.999" "${output_default}" "${output_cms0}" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "libwebp default cms behavior matches explicit cms=0 for static ICC input"
exit 0
