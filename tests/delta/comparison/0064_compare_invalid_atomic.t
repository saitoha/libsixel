#!/bin/sh
# Verify compare invalid atomic.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0064_compare_invalid_atomic" >/dev/null || {
    echo "not ok 1 - compare invalid atomic"
    exit 0
}

echo "ok 1 - compare invalid atomic"
exit 0
