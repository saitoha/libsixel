#!/bin/sh
# Verify exact binning executes through K-means and commits its frame state.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --palette-binning=exact -Qauto -dnone -p16 "-~none" \
    -Lbuiltin -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" 2>&1 >/dev/null) || {
    echo "not ok 1 - exact palette binning encode failed"
    exit 0
}

test "${trace#*LSXBPS1|quantizer_requested=0|quantizer_effective=2|quantizer_origin=auto|quantizer_phase=executed|quantizer_reason=quantizer-capability|binning_requested=exact|binning_effective=exact|binning_origin=explicit|binning_phase=executed|binning_reason=explicit|points=*}" != "${trace}" || {
    echo "not ok 1 - exact palette binning state was not committed"
    exit 0
}

echo "ok 1 - exact palette binning executes through K-means"
exit 0
