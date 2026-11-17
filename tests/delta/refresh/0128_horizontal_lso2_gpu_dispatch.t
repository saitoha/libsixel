#!/bin/sh
# Verify horizontal lso2 gpu dispatch.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    delta/refresh/0128_horizontal_lso2_gpu_dispatch || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - horizontal lso2 gpu dispatch"
    exit 0
}
echo "ok 1 - horizontal lso2 gpu dispatch"
exit 0
