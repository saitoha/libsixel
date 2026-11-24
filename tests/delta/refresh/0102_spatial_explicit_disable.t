#!/bin/sh
# Verify J0 disables spatial refresh.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0102_spatial_explicit_disable || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP required runtime is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - J0 disables spatial refresh"
    exit 0
}
printf '\n%s\n' "ok 1 - J0 disables spatial refresh"
exit 0
