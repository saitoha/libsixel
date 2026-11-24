#!/bin/sh
# Verify adaptive off gray history.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0078_adaptive_off_gray_history || status=$?
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - adaptive off gray history"
    exit 0
}
printf '\n%s\n' "ok 1 - adaptive off gray history"
exit 0
