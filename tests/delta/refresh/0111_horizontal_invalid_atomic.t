#!/bin/sh
# Verify horizontal invalid atomic.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0111_horizontal_invalid_atomic || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - horizontal invalid atomic"
    exit 0
}
echo "ok 1 - horizontal invalid atomic"
exit 0
