#!/bin/sh
# Verify the colorspace threshold environment reaches its consumer.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=runtime_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS=257 \
    -W oklab -o /dev/null \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-32.png" 2>&1) || {
    echo "not ok 1 - colorspace threshold conversion failed"
    exit 0
}

test "${trace#*LSXRT1|colorspace_min=257*}" != "${trace}" || {
    echo "not ok 1 - colorspace threshold missed its consumer"
    exit 0
}

echo "ok 1 - colorspace threshold reaches its consumer"
exit 0
