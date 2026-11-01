#!/bin/sh
# Verify GPU partial rectangle auto rectangle cutoff.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta/gpu-rectangle/0019_auto_rectangle_cutoff || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - GPU partial rectangle auto rectangle cutoff"
    exit 0
}
echo "ok 1 - GPU partial rectangle auto rectangle cutoff"
exit 0
