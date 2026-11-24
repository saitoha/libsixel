#!/bin/sh
# Verify uncommitted frame does not advance refresh.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-refresh/0004_uncommitted_frame || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP GPU palette backend is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - uncommitted frame does not advance refresh"
    exit 0
}
printf '\n%s\n' "ok 1 - uncommitted frame does not advance refresh"
exit 0
