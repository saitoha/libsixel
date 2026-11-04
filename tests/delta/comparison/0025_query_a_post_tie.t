#!/bin/sh
# Verify query a post tie.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0025_query_a_post_tie" >/dev/null || {
    echo "not ok 1 - query a post tie"
    exit 0
}

echo "ok 1 - query a post tie"
exit 0
