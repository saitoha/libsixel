#!/bin/sh
# Verify floor change restart.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0074_floor_change_restart || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - floor change restart"
    exit 0
}
echo "ok 1 - floor change restart"
exit 0
