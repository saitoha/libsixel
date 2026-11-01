#!/bin/sh
# Verify the sample target reaches the palette sampling consumer.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgb.png"

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_PALETTE_SAMPLE_TARGET=128 \
    --sampling-policy=adaptive-grid \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok 1 - sample target conversion failed"
    exit 0
}

case "${trace}" in *LSXSMP1\|override=1\|target=128\|stride=6**) true ;; *) false ;; esac || {
    echo "not ok 1 - sample target missed its consumer"
    exit 0
}


echo "ok 1 - sample target reaches the palette consumer"
exit 0
