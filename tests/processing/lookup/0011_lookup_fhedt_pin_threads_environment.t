#!/bin/sh
# Pin the canonical and legacy FHEDT affinity environment contract.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --env "SIXEL_FHEDT_PIN_THREADS=0" \
    --env "SIXEL_LOOKUP_FHEDT_PIN_THREADS=1" -p 16 "-~fhedt" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "FHEDT pin_threads environment conversion failed"
    exit 0
}
test "${trace#*LSXFHD1|precision=8bit|*pin_threads=1*}" != "${trace}" || {
    echo "not ok" 1 - "canonical FHEDT pin_threads did not win"
    exit 0
}

echo "ok" 1 - "canonical FHEDT pin_threads precedes its legacy alias"
exit 0
