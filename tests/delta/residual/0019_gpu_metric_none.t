#!/bin/sh
# Verify gpu metric none.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v

# Initialize the capability prerequisite and dispatch in one timed process.
status=0

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette delta-residual/0019_gpu_metric_none || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - gpu metric none"
    exit 0
}
echo "ok 1 - gpu metric none"
exit 0
