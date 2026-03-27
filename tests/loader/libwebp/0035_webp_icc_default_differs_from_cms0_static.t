#!/bin/sh
# Verify libwebp default cms behavior differs from cms=1 for static ICC input.

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
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_webp_icc="${TOP_SRCDIR}/tests/data/inputs/formats/palette_lossless_embedded_a98_icc.webp"
output_default="${ARTIFACT_LOCAL_DIR}/webp-icc-default-static-diff.sixel"
output_cms1="${ARTIFACT_LOCAL_DIR}/webp-icc-cms1-static-diff.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp! "${input_webp_icc}" >"${output_default}" || {
    echo "not ok" 1 - "libwebp default decode failed for ICC input (static)"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=auto! "${input_webp_icc}" >"${output_cms1}" || {
    echo "not ok" 1 - "libwebp cms=1 decode failed for ICC input (static)"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.999" "${output_default}" "${output_cms1}" 2>&1) || lsqa_status=$?

test "${lsqa_status-0}" -eq 5 || {
    echo "not ok" 1 - "libwebp default and cms=1 static outputs were not distinguishable: ${lsqa_msg-}"
    exit 0
}

echo "ok" 1 - "libwebp default cms behavior differs from cms=1 for static ICC input"
exit 0
