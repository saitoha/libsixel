#!/bin/sh
# Verify exact binning is rejected before work for unsupported quantizers.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

status=0
trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --palette-binning=exact -Qheckbert \
    -dnone -p16 "-~none" -Lbuiltin -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" 2>&1 >/dev/null) || status=$?

test "${status}" -eq 2 || {
    echo "not ok 1 - unsupported exact quantizer was accepted"
    exit 0
}

test "${trace#*palette binning policy is unsupported by the selected quantizer*}" != \
    "${trace}" || {
    echo "not ok 1 - unsupported exact quantizer diagnostic is missing"
    exit 0
}

test "${trace#*LSXSPL1|requested=auto|effective=unset|source=none|origin=auto|phase=unresolved|reason=none|threads=1|heavy=0|budget_async=0|job_ready=0*}" != "${trace}" || {
    echo "not ok 1 - unsupported exact quantizer changed sampling state"
    exit 0
}

echo "ok 1 - unsupported exact quantizer fails during preflight"
exit 0
