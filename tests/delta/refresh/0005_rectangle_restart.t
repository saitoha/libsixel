#!/bin/sh
# Verify rectangle change restarts refresh.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-refresh/0005_rectangle_restart || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - rectangle change restarts refresh"
    exit 0
}
echo "ok 1 - rectangle change restarts refresh"
exit 0
