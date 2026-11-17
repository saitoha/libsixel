#!/bin/sh
# Verify horizontal periods rejected.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0121_horizontal_periods_rejected || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - horizontal periods rejected"
    exit 0
}
echo "ok 1 - horizontal periods rejected"
exit 0
