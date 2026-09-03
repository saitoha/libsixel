#!/bin/sh
# Pin the existing FHEDT refinement environment contract.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --env "SIXEL_LOOKUP_FHEDT_REFINE=0" -p 16 "-~fhedt" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "FHEDT refine environment conversion failed"
    exit 0
}
test "${trace#*LSXLUT1|policy=fhedt|*refine=0*}" != "${trace}" || {
    echo "not ok" 1 - "FHEDT refine environment was not consumed"
    exit 0
}

echo "ok" 1 - "FHEDT refine environment remains effective"
exit 0
