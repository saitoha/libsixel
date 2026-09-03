#!/bin/sh
# Verify the SIMD level environment reaches CPU dispatch.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=runtime_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_SIMD_LEVEL=scalar \
    -w 31 -o /dev/null \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-32.png" 2>&1) || {
    echo "not ok 1 - SIMD level conversion failed"
    exit 0
}

test "${trace#*LSXRT1|simd_cap=0|*effective=0*}" != "${trace}" || {
    echo "not ok 1 - SIMD level missed CPU dispatch"
    exit 0
}

echo "ok 1 - SIMD level reaches CPU dispatch"
exit 0
