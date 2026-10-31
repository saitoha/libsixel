#!/bin/sh
# Verify staggered coverage.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-refresh/0002_staggered_coverage || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - staggered coverage"
    exit 0
}
echo "ok 1 - staggered coverage"
exit 0
