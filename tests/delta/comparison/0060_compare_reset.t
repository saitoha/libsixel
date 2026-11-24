#!/bin/sh
# Verify compare reset.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0060_compare_reset" >/dev/null || {
    printf '\n%s\n' "not ok 1 - compare reset"
    exit 0
}

printf '\n%s\n' "ok 1 - compare reset"
exit 0
