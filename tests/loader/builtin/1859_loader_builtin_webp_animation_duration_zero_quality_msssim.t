#!/bin/sh
# TAP test confirming duration=0 animation keeps builtin/libwebp quality parity.

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

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-min-duration0.webp"
output_builtin="${ARTIFACT_ROOT}/${0##*/}.builtin.png"
output_libwebp="${ARTIFACT_ROOT}/${0##*/}.libwebp.png"
lsqa_msg=''

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -S -L builtin! -ldisable \
    -o "${output_builtin}" "${input_webp}" >/dev/null || {
    echo "not ok" 1 - "builtin duration=0 animation quality decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -S -L libwebp! -ldisable \
    -o "${output_libwebp}" "${input_webp}" >/dev/null || {
    echo "not ok" 1 - "libwebp duration=0 animation quality decode failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM \
    -b "MS-SSIM:0.98" "${output_libwebp}" "${output_builtin}" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "duration=0 animation quality keeps MS-SSIM >= 0.98"
exit 0
