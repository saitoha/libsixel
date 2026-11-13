#!/bin/sh
# Verify floor runtime cap.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0070_floor_runtime_cap || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - floor runtime cap"
    exit 0
}
echo "ok 1 - floor runtime cap"
exit 0
