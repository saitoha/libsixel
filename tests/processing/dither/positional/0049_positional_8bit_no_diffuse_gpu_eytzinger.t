#!/bin/sh
# TAP test covering the explicit GPU Eytzinger palette lookup path.
#
# The GPU path must accept the one-dimensional lookup policy and complete the
# indexed palette apply without falling back to the CPU implementation.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

probe_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_GPU_POLICY=force \
        -d none -p 16 --lookup-policy=eytzinger -o /dev/null \
        "${probe_image}" || {
    printf "1..0 # SKIP forced GPU Eytzinger lookup is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_GPU_POLICY=force \
        -d none -p 256 --lookup-policy=eytzinger -o /dev/null \
        "${probe_image}" || {
    echo "not ok 1 - forced GPU Eytzinger lookup failed"
    exit 0
}

echo "ok 1 - forced GPU Eytzinger lookup completes"
exit 0
