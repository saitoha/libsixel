#!/bin/sh
# Verify GPU partial rectangle blockcull.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette delta/gpu-rectangle/0003_blockcull || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - GPU partial rectangle blockcull"
    exit 0
}
echo "ok 1 - GPU partial rectangle blockcull"
exit 0
