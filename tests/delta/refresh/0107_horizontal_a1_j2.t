#!/bin/sh
# Verify horizontal a1 j2.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0107_horizontal_a1_j2 || status=$?
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - horizontal a1 j2"
    exit 0
}
printf '\n%s\n' "ok 1 - horizontal a1 j2"
exit 0
