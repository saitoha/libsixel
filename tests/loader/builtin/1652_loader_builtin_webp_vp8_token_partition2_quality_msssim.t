#!/bin/sh
# TAP test confirming VP8 token partition=2 quality against libwebp.
# Fixture source: webmproject/libwebp-test-data@06ddd96e276c2c638a72d39d3c0f340afd61978c
# Fixture SHA256: 25cd4540f189f61ab0119f8f26e3dc28ba1a7840843b205389948dc3019eee6d

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/inputs/vp80-04-partitions-1404.webp"
output_builtin="${ARTIFACT_ROOT}/${0##*/}.builtin.png"
output_libwebp="${ARTIFACT_ROOT}/${0##*/}.libwebp.png"
lsqa_msg=''

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! -o "${output_builtin}" \
    "${input_webp}" >/dev/null || {
    echo "not ok" 1 - "builtin VP8 token partition=2 decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -o "${output_libwebp}" \
    "${input_webp}" >/dev/null || {
    echo "not ok" 1 - "libwebp VP8 token partition=2 decode failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM \
    -b "MS-SSIM:0.98" "${output_libwebp}" "${output_builtin}" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "builtin VP8 token partition=2 quality keeps MS-SSIM >= 0.98"
exit 0
