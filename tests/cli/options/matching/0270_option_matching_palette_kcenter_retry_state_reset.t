#!/bin/sh
# Verify a failed float K-center attempt retries from RGB888 and fresh state.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    _SIXEL_TEST_PALETTE_QUANTIZER_POST_BUILD_FAILURE=kcenter-float32 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --palette-binning=hard -Qcenter --precision=float32 \
    -dnone -p16 "-~none" -Lbuiltin -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" 2>&1 >/dev/null) || {
    echo "not ok 1 - legacy K-center retry failed"
    exit 0
}

test "${trace#*LSXBPS1|quantizer_requested=4|quantizer_effective=4|quantizer_origin=explicit|quantizer_phase=executed|quantizer_reason=explicit|binning_requested=hard|binning_effective=hard|binning_origin=explicit|binning_phase=executed|binning_reason=explicit|points=*|quantizer_retries=1*}" != "${trace}" || {
    echo "not ok 1 - legacy retry did not publish executed K-center state"
    exit 0
}

echo "ok 1 - K-center retry uses RGB888 and fresh binning state"
exit 0
