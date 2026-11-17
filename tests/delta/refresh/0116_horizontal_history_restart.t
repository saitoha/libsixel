#!/bin/sh
# Verify horizontal history restart.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0116_horizontal_history_restart || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - horizontal history restart"
    exit 0
}
echo "ok 1 - horizontal history restart"
exit 0
