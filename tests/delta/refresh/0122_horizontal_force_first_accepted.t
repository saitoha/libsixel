#!/bin/sh
# Verify horizontal force first accepted.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0122_horizontal_force_first_accepted || status=$?
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - horizontal force first accepted"
    exit 0
}
printf '\n%s\n' "ok 1 - horizontal force first accepted"
exit 0
