#!/bin/sh
# Verify compare full reject.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0065_compare_full_reject" >/dev/null || {
    echo "not ok 1 - compare full reject"
    exit 0
}

echo "ok 1 - compare full reject"
exit 0
