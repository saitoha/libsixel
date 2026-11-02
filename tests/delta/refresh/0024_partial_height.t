#!/bin/sh
# Verify partial height.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-refresh/0024_partial_height || {
    echo "not ok 1 - partial height"
    exit 0
}
echo "ok 1 - partial height"
exit 0
