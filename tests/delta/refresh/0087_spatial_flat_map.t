#!/bin/sh
# Verify spatial flat map.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0087_spatial_flat_map || status=$?
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - spatial flat map"
    exit 0
}
printf '\n%s\n' "ok 1 - spatial flat map"
exit 0
