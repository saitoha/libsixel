#!/bin/sh
# Verify curve1 error17 period.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0061_curve1_error17_period || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - curve1 error17 period"
    exit 0
}
echo "ok 1 - curve1 error17 period"
exit 0
