#!/bin/sh
# Verify partial latest.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-refresh/0013_partial_latest || {
    echo "not ok 1 - partial latest"
    exit 0
}
echo "ok 1 - partial latest"
exit 0
