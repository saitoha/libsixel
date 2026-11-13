#!/bin/sh
# Verify live carry changes indexes.
# Policy: docs/encoder/palette-application/dithering/sierra2-gpu.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "dither/sierra2-gpu/0010_live_carry_changes_indexes" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - live carry changes indexes"
    exit 0
}
echo "ok 1 - live carry changes indexes"
exit 0
