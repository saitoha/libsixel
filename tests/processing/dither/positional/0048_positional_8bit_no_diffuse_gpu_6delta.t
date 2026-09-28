#!/bin/sh
# TAP test covering GPU palette apply with 6delta accumulation.
#
# The direct helper keeps this regression focused on the GPU contract:
# unchanged pixels must become the accumulation keycolor and must be recorded
# in the result mask that the encoder later uses to retain the previous RGB
# plane.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

probe_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_GPU_POLICY=force \
        -d none -p 16 --lookup-policy=none -o /dev/null "${probe_image}" || {
    printf "1..0 # SKIP forced GPU palette apply is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    dither/positional/0048_gpu_6delta >/dev/null || {
    echo "not ok 1 - GPU 6delta accumulation result mismatch"
    exit 0
}

echo "ok 1 - GPU palette apply records 6delta accumulation"
exit 0
