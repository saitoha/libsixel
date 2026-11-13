#!/bin/sh
# Verify wire x rgb eytzinger.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup eytzinger \
    "dither/x-dither/0050_wire_x_rgb_eytzinger" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - wire x rgb eytzinger"
    exit 0
}
echo "ok 1 - wire x rgb eytzinger"
exit 0
