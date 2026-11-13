#!/bin/sh
# Verify spatial async latest snapshot.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "ok 1 # SKIP threading is disabled"
    exit 0
}
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0089_spatial_async_latest_snapshot || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required runtime is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - spatial async latest snapshot"
    exit 0
}
echo "ok 1 - spatial async latest snapshot"
exit 0
