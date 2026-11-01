#!/bin/sh
# Verify the snap policy environment reaches the palette snap consumer.

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
    --env SIXEL_PALETTE_SNAP_POLICY=reversible \
    -p 16 -Qkmeans -W oklab \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok 1 - snap policy conversion failed"
    exit 0
}

case "${trace}" in *LSXSNP1\|policy=reversible**) true ;; *) false ;; esac || {
    echo "not ok 1 - snap policy missed its consumer"
    exit 0
}


echo "ok 1 - snap policy reaches the palette consumer"
exit 0
