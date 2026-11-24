#!/bin/sh
# Verify spatial blue cpu keep.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0096_spatial_blue_cpu_keep || status=$?
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - spatial blue cpu keep"
    exit 0
}
printf '\n%s\n' "ok 1 - spatial blue cpu keep"
exit 0
