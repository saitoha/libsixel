#!/bin/sh
# Verify compare invalid environment.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0063_compare_invalid_environment" >/dev/null || {
    echo "not ok 1 - compare invalid environment"
    exit 0
}

echo "ok 1 - compare invalid environment"
exit 0
