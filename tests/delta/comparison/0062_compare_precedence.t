#!/bin/sh
# Verify compare precedence.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0062_compare_precedence" >/dev/null || {
    printf '\n%s\n' "not ok 1 - compare precedence"
    exit 0
}

printf '\n%s\n' "ok 1 - compare precedence"
exit 0
