#!/bin/sh
# Policy: docs/encoder/palette-application/dithering/phased.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" phased/cancel_before || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP Metal device unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - phased cancel before"
    exit 0
}
echo "ok 1 - phased cancel before"
exit 0
