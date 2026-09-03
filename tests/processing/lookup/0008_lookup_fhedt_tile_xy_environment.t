#!/bin/sh
# Pin the canonical and legacy FHEDT XY tile environment contract.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --env "SIXEL_FHEDT_TILE_XY=9" \
    --env "SIXEL_LOOKUP_FHEDT_TILE_XY=5" -p 16 "-~fhedt" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "FHEDT tile_xy environment conversion failed"
    exit 0
}
test "${trace#*LSXFHD1|precision=8bit|tile_xy=5*}" != "${trace}" || {
    echo "not ok" 1 - "canonical FHEDT tile_xy did not win"
    exit 0
}

echo "ok" 1 - "canonical FHEDT tile_xy precedes its legacy alias"
exit 0
