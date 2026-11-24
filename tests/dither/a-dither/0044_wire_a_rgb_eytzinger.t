#!/bin/sh
# Verify wire a rgb eytzinger.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup eytzinger \
    "dither/a-dither/0044_wire_a_rgb_eytzinger" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - wire a rgb eytzinger"
    exit 0
}
printf '\n%s\n' "ok 1 - wire a rgb eytzinger"
exit 0
