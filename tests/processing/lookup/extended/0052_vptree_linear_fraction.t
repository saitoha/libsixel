#!/bin/sh
# Verify vptree linear fraction.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/extended/0001_extended_lookup fraction 3 1 || {
    echo "not ok 1 - vptree linear fraction"
    exit 0
}

echo "ok 1 - vptree linear fraction"
exit 0
