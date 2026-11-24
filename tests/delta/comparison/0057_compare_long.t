#!/bin/sh
# Verify compare long.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0057_compare_long" >/dev/null || {
    printf '\n%s\n' "not ok 1 - compare long"
    exit 0
}

printf '\n%s\n' "ok 1 - compare long"
exit 0
