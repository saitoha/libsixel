#!/bin/sh
# Verify horizontal policy reset.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0118_horizontal_policy_reset || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - horizontal policy reset"
    exit 0
}
echo "ok 1 - horizontal policy reset"
exit 0
