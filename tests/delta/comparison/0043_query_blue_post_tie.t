#!/bin/sh
# Verify query blue post tie.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0043_query_blue_post_tie" >/dev/null || {
    echo "not ok 1 - query blue post tie"
    exit 0
}

echo "ok 1 - query blue post tie"
exit 0
