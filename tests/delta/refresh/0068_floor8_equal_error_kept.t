#!/bin/sh
# Verify floor8 equal error kept.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0068_floor8_equal_error_kept || status=$?
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - floor8 equal error kept"
    exit 0
}
printf '\n%s\n' "ok 1 - floor8 equal error kept"
exit 0
