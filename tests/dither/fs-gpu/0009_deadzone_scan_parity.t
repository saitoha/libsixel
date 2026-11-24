#!/bin/sh
# Verify deadzone scan parity.
# Policy: docs/encoder/palette-application/dithering/fs-gpu.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "dither/fs-gpu/0009_deadzone_scan_parity" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - deadzone scan parity"
    exit 0
}
printf '\n%s\n' "ok 1 - deadzone scan parity"
exit 0
