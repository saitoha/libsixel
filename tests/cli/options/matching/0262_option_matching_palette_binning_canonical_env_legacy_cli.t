#!/bin/sh
# Verify the legacy CLI alias overrides the canonical environment default.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --env SIXEL_PALETTE_BINNING=hard -Qkmeans:binning=soft \
    -dnone -p16 "-~none" -Lbuiltin -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" 2>&1 >/dev/null) || {
    echo "not ok 1 - legacy CLI alias did not override canonical environment"
    exit 0
}

test "${trace#*LSXBIN1|requested=soft|origin=legacy-alias|quantizer_requested=2|quantizer_effective=2|phase=quantizer*}" != "${trace}" || {
    echo "not ok 1 - canonical environment overrode legacy CLI alias"
    exit 0
}

echo "ok 1 - legacy CLI alias overrides canonical environment"
exit 0
