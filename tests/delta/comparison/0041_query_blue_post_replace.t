#!/bin/sh
# Verify query blue post replace.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0041_query_blue_post_replace" >/dev/null || {
    printf '\n%s\n' "not ok 1 - query blue post replace"
    exit 0
}

printf '\n%s\n' "ok 1 - query blue post replace"
exit 0
