#!/bin/sh
# Verify query x post replace.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0032_query_x_post_replace" >/dev/null || {
    echo "not ok 1 - query x post replace"
    exit 0
}

echo "ok 1 - query x post replace"
exit 0
