#!/bin/sh
# Verify compare invalid execution.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0055_compare_invalid_execution" >/dev/null || {
    printf '\n%s\n' "not ok 1 - compare invalid execution"
    exit 0
}

printf '\n%s\n' "ok 1 - compare invalid execution"
exit 0
