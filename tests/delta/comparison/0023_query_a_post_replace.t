#!/bin/sh
# Verify query a post replace.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0023_query_a_post_replace" >/dev/null || {
    printf '\n%s\n' "not ok 1 - query a post replace"
    exit 0
}

printf '\n%s\n' "ok 1 - query a post replace"
exit 0
