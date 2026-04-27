#!/bin/sh
# TAP test confirming rgb_mab_valid.webp static quality against libwebp.

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

input_webp="${TOP_SRCDIR}/tests/data/colormgmt/input/custom/rgb_mab_valid.webp"
output_builtin="${ARTIFACT_ROOT}/${0##*/}.builtin.png"
output_libwebp="${ARTIFACT_ROOT}/${0##*/}.libwebp.png"
lsqa_msg=''

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -S -L builtin! -o "${output_builtin}" \
    "${input_webp}" >/dev/null || {
    echo "not ok" 1 - "builtin rgb_mab_valid.webp static decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -S -L libwebp! -o "${output_libwebp}" \
    "${input_webp}" >/dev/null || {
    echo "not ok" 1 - "libwebp rgb_mab_valid.webp static decode failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM \
    -b "MS-SSIM:0.98" "${output_libwebp}" "${output_builtin}" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "builtin rgb_mab_valid.webp quality keeps MS-SSIM >= 0.98"
exit 0
