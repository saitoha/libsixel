#!/bin/sh
# Verify horizontal a1 j4.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0108_horizontal_a1_j4 || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - horizontal a1 j4"
    exit 0
}
echo "ok 1 - horizontal a1 j4"
exit 0
