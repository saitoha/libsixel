#!/bin/sh
# Verify query blue float post.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0044_query_blue_float_post" >/dev/null || {
    echo "not ok 1 - query blue float post"
    exit 0
}

echo "ok 1 - query blue float post"
exit 0
