#!/bin/sh
# Verify explicit hard binning resolves the auto quantizer to k-means.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --binning-policy=hard -Qauto -dnone -p16 "-~none" \
    -Lbuiltin -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" 2>&1 >/dev/null) || {
    echo "not ok 1 - hard binning with auto quantizer failed"
    exit 0
}

test "${trace#*LSXBIN1|requested=hard|origin=explicit|quantizer_requested=0|quantizer_effective=2|phase=quantizer*}" != "${trace}" || {
    echo "not ok 1 - hard binning did not select k-means"
    exit 0
}

echo "ok 1 - hard binning resolves the auto quantizer to k-means"
exit 0
