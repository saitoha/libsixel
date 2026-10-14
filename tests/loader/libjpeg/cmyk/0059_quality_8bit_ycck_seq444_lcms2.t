#!/bin/sh
# Policy: docs/loader/libjpeg.md
# Compare final SIXEL against an independent Decimal affine ICC image.
# Assess the explicitly sized encoder output, after source decoding and
# nearest resampling but before any post-encoding enlargement. The independent
# reference uses the same integer replication; native sample tests stay small.
# Regenerate: python3 tests/data/loader/quality/generate_references.py

set -eux
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" jpeg-cmyk/precision \
    "${TOP_SRCDIR}" 0 3 0 1>&2 || result=$?
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
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -r nearest -w 192 -h 192 --threads=1 \
    -L 'libjpeg:cms_engine=lcms2:cms_intent=relative!!' \
    "${TOP_SRCDIR}/tests/data/color-numeric/jpeg-precision-8bit-ycck-seq444-affine.jpg" \
    | ${SIXEL_RUNTIME-} "${LSQA_PATH}" --cms-engine=none \
    -m MS-SSIM -b MS-SSIM:0.98 \
    "${TOP_SRCDIR}/tests/data/loader/quality/79bef44d-jpeg-precision-8bit-ycck-seq444-reference.ppm" \
    - 1>&2 || {
    echo "not ok 1 - independent 8bit-ycck-seq444 lcms2 quality below 0.98"
    exit 0
}
echo "ok 1 - independent 8bit-ycck-seq444 lcms2 quality"
exit 0
