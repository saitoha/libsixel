#!/bin/sh
# Verify partial ahead.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-refresh/0012_partial_ahead || {
    echo "not ok 1 - partial ahead"
    exit 0
}
echo "ok 1 - partial ahead"
exit 0
