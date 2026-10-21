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

echo "1..1"
set -v

# Keep capability initialization and real dispatch in one timed process.
status=0

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    dither/positional/0048_gpu_6delta >/dev/null || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - GPU 6delta accumulation result mismatch"
    exit 0
}

echo "ok 1 - GPU palette apply records 6delta accumulation"
exit 0
