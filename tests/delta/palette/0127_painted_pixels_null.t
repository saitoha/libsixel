#!/bin/sh
# Verify painted pixels null.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/palette/0127_painted_pixels_null" || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - painted pixels null"
    exit 0
}
echo "ok 1 - painted pixels null"
exit 0
