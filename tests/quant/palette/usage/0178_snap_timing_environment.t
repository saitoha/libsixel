#!/bin/sh
# Verify the snap timing policy reaches the palette snap consumer.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/snake-32.png"

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_PALETTE_SNAP_TIMING_POLICY=all \
    -p 16 -Qkmeans -_nearest -W oklab \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok 1 - snap timing conversion failed"
    exit 0
}

case "${trace}" in *LSXSNP1\|timing=all**) true ;; *) false ;; esac || {
    echo "not ok 1 - snap timing missed its consumer"
    exit 0
}


echo "ok 1 - snap timing reaches the palette consumer"
exit 0
