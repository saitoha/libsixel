#!/bin/sh
# TAP test confirming builtin VP8L transform subimages use subsampled height.

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

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/webp-lossless-rgb64-cwebp-transform-subsample.webp"
output_builtin="${ARTIFACT_ROOT}/${0##*/}.builtin.png"
output_libwebp="${ARTIFACT_ROOT}/${0##*/}.libwebp.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -I -L builtin! \
    -o "${output_builtin}" "${input_webp}" >/dev/null || {
    echo "not ok" 1 - "builtin VP8L transform-subsample fixture decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -I -L libwebp! \
    -o "${output_libwebp}" "${input_webp}" >/dev/null || {
    echo "not ok" 1 - "libwebp VP8L transform-subsample fixture decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.999" \
    "${output_libwebp}" "${output_builtin}" >/dev/null || {
    echo "not ok" 1 - "builtin VP8L transform-subsample MS-SSIM is too low"
    exit 0
}

echo "ok" 1 - "builtin VP8L transform-subsample quality keeps MS-SSIM >= 0.999"
exit 0
