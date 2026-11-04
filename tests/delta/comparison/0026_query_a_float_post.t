#!/bin/sh
# Verify query a float post.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0026_query_a_float_post" >/dev/null || {
    echo "not ok 1 - query a float post"
    exit 0
}

echo "ok 1 - query a float post"
exit 0
