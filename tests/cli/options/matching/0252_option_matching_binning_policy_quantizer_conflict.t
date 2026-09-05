#!/bin/sh
# Verify a raw-only quantizer rejects explicit hard binning.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

status=0
message=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=4 \
    --binning-policy=hard -Qheckbert -dnone -p16 "-~none" \
    -Lbuiltin -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" 2>&1 >/dev/null) || status=$?

test "${status}" -eq 2 || {
    echo "not ok 1 - incompatible quantizer exit status mismatch"
    exit 0
}

test "${message#*unsupported by the selected quantizer*}" != "${message}" || {
    echo "not ok 1 - incompatible quantizer diagnostic is missing"
    exit 0
}

test "${message#*LSXPFB1*}" = "${message}" || {
    echo "not ok 1 - incompatible policy entered sampling fallback"
    exit 0
}

echo "ok 1 - explicit hard binning rejects a raw-only quantizer"
exit 0
