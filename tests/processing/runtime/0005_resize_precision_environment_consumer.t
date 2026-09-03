#!/bin/sh
# Verify the resize precision environment reaches the planner.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=runtime_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_PLANNER_RESIZE_PRECISION_MODE=3 \
    -w 31 -o /dev/null \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-32.png" 2>&1) || {
    echo "not ok 1 - resize precision conversion failed"
    exit 0
}

test "${trace#*LSXRT1|resize_precision=3*}" != "${trace}" || {
    echo "not ok 1 - resize precision missed the planner"
    exit 0
}

echo "ok 1 - resize precision reaches the planner"
exit 0
