#!/bin/sh
# Verify query x post tie.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0034_query_x_post_tie" >/dev/null || {
    echo "not ok 1 - query x post tie"
    exit 0
}

echo "ok 1 - query x post tie"
exit 0
