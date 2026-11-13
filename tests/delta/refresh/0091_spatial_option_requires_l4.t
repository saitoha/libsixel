#!/bin/sh
# Verify spatial option requires l4.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0091_spatial_option_requires_l4 || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - spatial option requires l4"
    exit 0
}
echo "ok 1 - spatial option requires l4"
exit 0
