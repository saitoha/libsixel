#!/bin/sh
# Verify the merge channel factor reaches the palette merge consumer.

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
    --env SIXEL_PALETTE_CHANNEL_FACTOR_L=0.25 \
    --env SIXEL_PALETTE_MERGE_CHANNEL_FACTOR_L=0.75 \
    -p 16 -Qheckbert -Fward -W oklab \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok 1 - merge channel factor conversion failed"
    exit 0
}

case "${trace}" in *LSXMRG1\|channel_l=0.75**) true ;; *) false ;; esac || {
    echo "not ok 1 - merge channel factor missed its consumer"
    exit 0
}


echo "ok 1 - merge channel factor reaches the palette consumer"
exit 0
