#!/bin/sh
# Verify GPU partial rectangle bluenoise.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette delta/gpu-rectangle/0008_bluenoise || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - GPU partial rectangle bluenoise"
    exit 0
}
echo "ok 1 - GPU partial rectangle bluenoise"
exit 0
