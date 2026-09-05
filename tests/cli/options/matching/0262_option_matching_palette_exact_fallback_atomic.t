#!/bin/sh
# Verify a solver fallback cannot weaken exact binning or publish failed state.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

status=0
trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    _SIXEL_TEST_PALETTE_QUANTIZER_FAILURE=kmeans \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --palette-binning=exact -Qkmeans \
    -dnone -p16 "-~none" -Lbuiltin -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" 2>&1 >/dev/null) || status=$?

test "${status}" -eq 2 || {
    echo "not ok 1 - exact binning accepted an incompatible fallback"
    exit 0
}

test "${trace#*palette fallback quantizer cannot consume*}" != \
    "${trace}" || {
    echo "not ok 1 - exact fallback diagnostic is missing"
    exit 0
}

test "${trace#*LSXBPS1|quantizer_requested=2|quantizer_effective=-1|quantizer_origin=explicit|quantizer_phase=unresolved|quantizer_reason=none|binning_requested=exact|binning_effective=unset|binning_origin=explicit|binning_phase=unresolved|binning_reason=none|points=0|quantizer_retries=0*}" != "${trace}" || {
    echo "not ok 1 - exact fallback published partial policy state"
    exit 0
}

echo "ok 1 - exact binning rejects fallback atomically"
exit 0
