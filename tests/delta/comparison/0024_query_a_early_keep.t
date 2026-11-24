#!/bin/sh
# Verify query a early keep.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0024_query_a_early_keep" >/dev/null || {
    printf '\n%s\n' "not ok 1 - query a early keep"
    exit 0
}

printf '\n%s\n' "ok 1 - query a early keep"
exit 0
