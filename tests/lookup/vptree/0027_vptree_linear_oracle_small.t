#!/bin/sh
# Verify vptree linear oracle_small.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/extended/0001_extended_lookup oracle_small 3 1 || {
    echo "not ok 1 - vptree linear oracle_small"
    exit 0
}

echo "ok 1 - vptree linear oracle_small"
exit 0
