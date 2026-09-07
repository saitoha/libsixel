#!/bin/sh
# Verify a compatible solver fallback commits only its successful attempt.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    _SIXEL_TEST_PALETTE_QUANTIZER_FAILURE=kmeans \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    -Qauto:binning_policy=none -Qkmeans \
    -dnone -p16 "-~none" -Lbuiltin -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" 2>&1 >/dev/null) || {
    echo "not ok 1 - compatible quantizer fallback failed"
    exit 0
}

test "${trace#*LSXBPS1|quantizer_requested=2|quantizer_effective=1|quantizer_origin=explicit|quantizer_phase=executed|quantizer_reason=fallback|binning_requested=none|binning_effective=none|binning_origin=explicit|binning_phase=executed|binning_reason=explicit|points=*}" != "${trace}" || {
    echo "not ok 1 - fallback published the failed quantizer attempt"
    exit 0
}

echo "ok 1 - quantizer fallback commits only the successful attempt"
exit 0
