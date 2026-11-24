#!/bin/sh
# Verify query x early keep.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0033_query_x_early_keep" >/dev/null || {
    printf '\n%s\n' "not ok 1 - query x early keep"
    exit 0
}

printf '\n%s\n' "ok 1 - query x early keep"
exit 0
