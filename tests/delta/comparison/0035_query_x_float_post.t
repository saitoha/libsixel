#!/bin/sh
# Verify query x float post.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0035_query_x_float_post" >/dev/null || {
    echo "not ok 1 - query x float post"
    exit 0
}

echo "ok 1 - query x float post"
exit 0
