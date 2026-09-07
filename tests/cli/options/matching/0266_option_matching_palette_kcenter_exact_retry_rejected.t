#!/bin/sh
# Verify exact float K-center never retries through lossy RGB888 input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

status=0
trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    _SIXEL_TEST_PALETTE_QUANTIZER_POST_BUILD_FAILURE=kcenter-float32 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    -Qauto:binning_policy=exact -Qcenter --precision=float32 \
    -dnone -p16 "-~none" -Lbuiltin -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" 2>&1 >/dev/null) || status=$?

test "${status}" -eq 2 || {
    echo "not ok 1 - exact float K-center entered the RGB888 retry"
    exit 0
}

test "${trace#*palette fallback quantizer cannot consume*}" != \
    "${trace}" || {
    echo "not ok 1 - exact float K-center diagnostic is missing"
    exit 0
}

test "${trace#*LSXBPS1|quantizer_requested=4|quantizer_effective=-1|quantizer_origin=explicit|quantizer_phase=unresolved|quantizer_reason=none|binning_requested=exact|binning_effective=unset|binning_origin=explicit|binning_phase=unresolved|binning_reason=none|points=0|quantizer_retries=0*}" != "${trace}" || {
    echo "not ok 1 - exact float K-center recorded a lossy retry"
    exit 0
}

echo "ok 1 - exact float K-center rejects the RGB888 retry"
exit 0
