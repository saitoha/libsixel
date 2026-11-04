#!/bin/sh
# Verify compare target.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0059_compare_target" >/dev/null || {
    echo "not ok 1 - compare target"
    exit 0
}

echo "ok 1 - compare target"
exit 0
