#!/bin/sh
# Verify horizontal a0 j5.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0105_horizontal_a0_j5 || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - horizontal a0 j5"
    exit 0
}
echo "ok 1 - horizontal a0 j5"
exit 0
