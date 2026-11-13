#!/bin/sh
# Verify curve1 error32 period.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0062_curve1_error32_period || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - curve1 error32 period"
    exit 0
}
echo "ok 1 - curve1 error32 period"
exit 0
