#!/bin/sh
# Verify the legacy k-means environment does not select k-means for -Qauto.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --env SIXEL_PALETTE_KMEANS_BINNING=hard \
    -Qauto -dnone -p16 "-~none" -Lbuiltin -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" 2>&1 >/dev/null) || {
    echo "not ok 1 - legacy binning environment encode failed"
    exit 0
}

test "${trace#*LSXBIN1|requested=auto|origin=default|quantizer_requested=0|quantizer_effective=1|phase=quantizer*}" != "${trace}" || {
    echo "not ok 1 - legacy k-means environment selected k-means"
    exit 0
}

echo "ok 1 - legacy k-means binning environment remains quantizer-scoped"
exit 0
