#!/bin/sh
# TAP test confirming static XMP fixture quality matches libwebp when disabled.
# Derived fixture: orientation_exif_o6_vp8_static_12x8.webp
# Replaced EXIF chunk with XMP chunk: tiff:Orientation="6".

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

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_xmp_o6_vp8_static_12x8.webp"
output_builtin="${ARTIFACT_ROOT}/${0##*/}.builtin.png"
output_libwebp="${ARTIFACT_ROOT}/${0##*/}.libwebp.png"
lsqa_msg=''

SIXEL_LOADER_BUILTIN_ORIENTATION=off
export SIXEL_LOADER_BUILTIN_ORIENTATION
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -S -L builtin! -o "${output_builtin}" \
    "${input_webp}" >/dev/null || {
    echo "not ok" 1 - "builtin static XMP fixture quality decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -S -L libwebp! -o "${output_libwebp}" \
    "${input_webp}" >/dev/null || {
    echo "not ok" 1 - "libwebp static XMP fixture quality decode failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM \
    -b "MS-SSIM:0.98" "${output_libwebp}" "${output_builtin}" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "builtin static XMP fixture quality keeps MS-SSIM >= 0.98"
exit 0
