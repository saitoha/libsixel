#!/bin/sh
# Verify spatial flat original period.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0086_spatial_flat_original_period || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - spatial flat original period"
    exit 0
}
echo "ok 1 - spatial flat original period"
exit 0
