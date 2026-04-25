#!/bin/sh
# TAP test confirming animated WebP start_frame replay keeps subrect state parity.

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

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossy-alpha-subrect-80x64-3frame-min.webp"
output_builtin="${ARTIFACT_ROOT}/${0##*/}.builtin.png"
output_libwebp="${ARTIFACT_ROOT}/${0##*/}.libwebp.png"
lsqa_msg=''

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_LOADER_ANIMATION_START_FRAME_NO=2 \
    -L builtin! -ldisable -o "${output_builtin}" "${input_webp}" >/dev/null || {
    echo "not ok" 1 - "builtin animated WebP subrect start_frame=2 decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_LOADER_ANIMATION_START_FRAME_NO=2 \
    -L libwebp! -ldisable -o "${output_libwebp}" "${input_webp}" >/dev/null || {
    echo "not ok" 1 - "libwebp animated WebP subrect start_frame=2 decode failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM \
    -b "MS-SSIM:0.98" "${output_libwebp}" "${output_builtin}" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "animated WebP subrect start_frame=2 keeps MS-SSIM >= 0.98"
exit 0
