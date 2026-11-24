#!/bin/sh
# Verify custom refresh band periods.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0100_custom_band_periods || status=$?
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - custom refresh band periods"
    exit 0
}
printf '\n%s\n' "ok 1 - custom refresh band periods"
exit 0
