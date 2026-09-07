#!/bin/sh
# Verify automatic K-means binning resolves to the measured hard profile.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    -Qauto:binning_policy=auto -Qkmeans -dnone -p16 "-~none" \
    -Lbuiltin -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" 2>&1 >/dev/null) || {
    echo "not ok 1 - automatic K-means binning failed"
    exit 0
}

test "${trace#*LSXBPS1|quantizer_requested=2|quantizer_effective=2|quantizer_origin=explicit|quantizer_phase=executed|quantizer_reason=explicit|binning_requested=auto|binning_effective=hard|binning_origin=explicit|binning_phase=executed|binning_reason=resource-profile|points=*}" != "${trace}" || {
    echo "not ok 1 - automatic K-means binning did not select hard"
    exit 0
}

echo "ok 1 - automatic K-means binning selects hard"
exit 0
