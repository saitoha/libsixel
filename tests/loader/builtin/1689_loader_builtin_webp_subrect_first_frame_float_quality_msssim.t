#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Compare the first subrect frame at matched float precision.
# The snapshot decoder returns the first SIXEL frame, not the animation.
# libwebp can hand off linear-f32 while builtin hands off RGB888: force
# both downstream paths to float and decode only the first frame.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    set -v
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is disabled in this build\n"
    set -v
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
    -L builtin! -.float32 --threads=1 -S -g -ldisable -o "${output_builtin}" "${input_webp}" >/dev/null || {
    echo "not ok" 1 - "builtin WebP first subrect frame decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L libwebp! -.float32 --threads=1 -S -g -ldisable -o "${output_libwebp}" "${input_webp}" >/dev/null || {
    echo "not ok" 1 - "libwebp WebP first subrect frame decode failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM \
    -b "MS-SSIM:0.98" "${output_libwebp}" "${output_builtin}" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "WebP first subrect frame at float keeps MS-SSIM >= 0.98"
exit 0
