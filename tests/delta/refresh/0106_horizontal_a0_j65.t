#!/bin/sh
# Verify horizontal a0 j65.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0106_horizontal_a0_j65 || status=$?
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - horizontal a0 j65"
    exit 0
}
printf '\n%s\n' "ok 1 - horizontal a0 j65"
exit 0
