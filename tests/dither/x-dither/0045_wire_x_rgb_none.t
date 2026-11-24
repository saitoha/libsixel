#!/bin/sh
# Verify wire x rgb none.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup none \
    "dither/x-dither/0045_wire_x_rgb_none" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - wire x rgb none"
    exit 0
}
printf '\n%s\n' "ok 1 - wire x rgb none"
exit 0
