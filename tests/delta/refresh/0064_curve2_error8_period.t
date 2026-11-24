#!/bin/sh
# Verify curve2 error8 period.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0064_curve2_error8_period || status=$?
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - curve2 error8 period"
    exit 0
}
printf '\n%s\n' "ok 1 - curve2 error8 period"
exit 0
