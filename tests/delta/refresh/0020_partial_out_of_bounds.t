#!/bin/sh
# Verify partial out of bounds.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-refresh/0020_partial_out_of_bounds || {
    echo "not ok 1 - partial out of bounds"
    exit 0
}
echo "ok 1 - partial out of bounds"
exit 0
