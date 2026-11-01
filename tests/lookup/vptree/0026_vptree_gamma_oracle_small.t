#!/bin/sh
# Verify vptree gamma oracle_small.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/extended/0001_extended_lookup oracle_small 3 0 || {
    echo "not ok 1 - vptree gamma oracle_small"
    exit 0
}

echo "ok 1 - vptree gamma oracle_small"
exit 0
