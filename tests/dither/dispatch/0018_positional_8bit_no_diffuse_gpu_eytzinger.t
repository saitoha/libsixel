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

echo "1..1"
set -v

# A failed conversion is a regression unless the lookup capability is absent.
conversion_status=0
probe_status=0
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_GPU_POLICY=force \
        -d none -p 256 --lookup-policy=eytzinger -o /dev/null \
        "${probe_image}" || conversion_status=$?
test "$conversion_status" -eq 0 ||
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --gpu-lookup-available eytzinger || probe_status=$?
test "$probe_status" -ne 77 || {
    echo "ok 1 # SKIP GPU Eytzinger lookup is unavailable"
    exit 0
}
test "$probe_status" -eq 0 || {
    echo "not ok 1 - GPU Eytzinger lookup probe failed"
    exit 0
}
test "$conversion_status" -eq 0 || {
    echo "not ok 1 - forced GPU Eytzinger lookup failed"
    exit 0
}

echo "ok 1 - forced GPU Eytzinger lookup completes"
exit 0
