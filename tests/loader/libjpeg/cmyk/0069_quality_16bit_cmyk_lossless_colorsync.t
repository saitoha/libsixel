#!/bin/sh
# Policy: docs/loader/libjpeg.md
# Compare final SIXEL against an independent Decimal affine ICC image.
set -eux
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" jpeg-cmyk/precision \
    "${TOP_SRCDIR}" 3 4 0 1>&2 || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required libjpeg precision API or CMS is unavailable"
    set -v
    exit 0
}
test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "not ok 1 - numeric route qualification failed"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    -L 'libjpeg:cms_engine=colorsync:cms_intent=relative!!' \
    "${TOP_SRCDIR}/tests/data/color-numeric/jpeg-precision-16bit-cmyk-lossless-affine.jpg" \
    | ${SIXEL_RUNTIME-} "${LSQA_PATH}" --cms-engine=none \
    -m MS-SSIM -b MS-SSIM:0.98 \
    "${TOP_SRCDIR}/tests/data/color-numeric/jpeg-precision-16bit-cmyk-lossless-reference.png" \
    - 1>&2 || {
    echo "not ok 1 - independent 16bit-cmyk-lossless colorsync quality below 0.98"
    exit 0
}
echo "ok 1 - independent 16bit-cmyk-lossless colorsync quality"
exit 0
