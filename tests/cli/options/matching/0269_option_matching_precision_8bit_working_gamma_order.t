#!/bin/sh
# Verify gamma working space restores the 8-bit precision request.
# Policy: docs/functionality/working-colorspace.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
precision_first=$(set +xv; SIXEL_FLOAT32_DITHER=0 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --precision=8bit -Wlinear -Wgamma -dnone -p16 \
    -v -o/dev/null "${input_image}" 2>&1) || {
    echo "not ok 1 - precision-first conversion failed"
    exit 0
}
test "${precision_first#*work=rgb888*}" != "${precision_first}" || {
    echo "not ok 1 - gamma did not restore precision-first 8-bit work"
    exit 0
}

colorspace_first=$(set +xv; SIXEL_FLOAT32_DITHER=0 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    -Wlinear --precision=8bit -Wgamma -dnone -p16 \
    -v -o/dev/null "${input_image}" 2>&1) || {
    echo "not ok 1 - colorspace-first conversion failed"
    exit 0
}
test "${colorspace_first#*work=rgb888*}" != "${colorspace_first}" || {
    echo "not ok 1 - gamma did not restore colorspace-first 8-bit work"
    exit 0
}

echo "ok 1 - gamma working space preserves 8-bit option order"
exit 0
