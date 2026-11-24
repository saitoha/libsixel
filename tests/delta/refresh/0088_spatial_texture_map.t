#!/bin/sh
# Verify spatial texture map.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0088_spatial_texture_map || status=$?
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - spatial texture map"
    exit 0
}
printf '\n%s\n' "ok 1 - spatial texture map"
exit 0
