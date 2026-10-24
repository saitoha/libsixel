#!/bin/sh
# Policy: docs/decoder/pipeline/README.md
# Verify normal columns patterns.
set -eux
echo "1..1"
set -v

SIXEL_THREADING_GLOBAL_BUDGET=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0094_normal_columns_patterns" || {
    echo "not ok 1 - normal_columns_patterns"
    exit 0
}
echo "ok 1 - normal_columns_patterns"
exit 0
