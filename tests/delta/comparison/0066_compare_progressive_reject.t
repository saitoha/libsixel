#!/bin/sh
# Verify compare progressive reject.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0066_compare_progressive_reject" >/dev/null || {
    echo "not ok 1 - compare progressive reject"
    exit 0
}

echo "ok 1 - compare progressive reject"
exit 0
