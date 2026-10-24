#!/bin/sh
# Verify gamma working space restores the 8-bit precision request.
# Policy: docs/encoder/palette-application/working-colorspace/README.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
precision_first=$(set +xv; SIXEL_PRECISION_POLICY=auto \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin:cms_engine=none! --threading-policy=greedy:global_budget=1 \
    --precision-policy=8bit -Wlinear -Wgamma -dnone -p16 \
    -v -o/dev/null "${input_image}" 2>&1) || {
    echo "not ok 1 - precision-first conversion failed"
    exit 0
}
case "${precision_first}" in *work=rgb888**) true ;; *) false ;; esac || {
    echo "not ok 1 - gamma did not restore precision-first 8-bit work"
    exit 0
}

colorspace_first=$(set +xv; SIXEL_PRECISION_POLICY=auto \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin:cms_engine=none! --threading-policy=greedy:global_budget=1 \
    -Wlinear --precision-policy=8bit -Wgamma -dnone -p16 \
    -v -o/dev/null "${input_image}" 2>&1) || {
    echo "not ok 1 - colorspace-first conversion failed"
    exit 0
}
case "${colorspace_first}" in *work=rgb888**) true ;; *) false ;; esac || {
    echo "not ok 1 - gamma did not restore colorspace-first 8-bit work"
    exit 0
}

echo "ok 1 - gamma working space preserves 8-bit option order"
exit 0
