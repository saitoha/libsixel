#!/bin/sh
# Verify raw binning preserves the historical auto quantizer selection.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --palette-binning=none \
    -Qauto -dnone -p16 "-~none" -Lbuiltin -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" 2>&1 >/dev/null) || {
    echo "not ok 1 - raw binning with auto quantizer failed"
    exit 0
}

test "${trace#*LSXBIN1|requested=none|origin=explicit|quantizer_requested=0|quantizer_effective=1|phase=quantizer*}" != "${trace}" || {
    echo "not ok 1 - raw binning changed auto quantizer compatibility"
    exit 0
}

echo "ok 1 - raw binning preserves auto quantizer compatibility"
exit 0
