#!/bin/sh
# Verify compare short.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0058_compare_short" >/dev/null || {
    echo "not ok 1 - compare short"
    exit 0
}

echo "ok 1 - compare short"
exit 0
