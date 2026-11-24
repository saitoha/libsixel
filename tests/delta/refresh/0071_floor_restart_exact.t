#!/bin/sh
# Verify floor restart exact.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0071_floor_restart_exact || status=$?
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - floor restart exact"
    exit 0
}
printf '\n%s\n' "ok 1 - floor restart exact"
exit 0
