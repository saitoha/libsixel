#!/bin/sh
# TAP test confirming animated EXIF+XMP fixture second-frame quality.
# Derived fixture: animated-lossy-8x8-2frame-min-exif-o6.webp
# Patched offsets: VP8X flags at 0x14 to include XMP, appended XMP orientation=3.

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

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossy-8x8-2frame-min-exif-o6-xmp-o3.webp"
output_builtin="${ARTIFACT_ROOT}/${0##*/}.builtin.png"
output_libwebp="${ARTIFACT_ROOT}/${0##*/}.libwebp.png"
lsqa_msg=''

SIXEL_LOADER_BUILTIN_ORIENTATION=on
export SIXEL_LOADER_BUILTIN_ORIENTATION
SIXEL_LOADER_ANIMATION_START_FRAME_NO=1
export SIXEL_LOADER_ANIMATION_START_FRAME_NO

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -S -L builtin! \
    -o "${output_builtin}" "${input_webp}" >/dev/null || {
    echo "not ok" 1 - "builtin animated EXIF+XMP second-frame quality decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -S -L libwebp! \
    -o "${output_libwebp}" "${input_webp}" >/dev/null || {
    echo "not ok" 1 - "libwebp animated EXIF+XMP second-frame quality decode failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM \
    -b "MS-SSIM:0.98" "${output_libwebp}" "${output_builtin}" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "builtin animated EXIF+XMP second-frame quality keeps MS-SSIM >= 0.98"
exit 0
