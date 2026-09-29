#!/bin/sh
# Verify vptree din99d fraction.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/extended/0001_extended_lookup fraction 3 4 || {
    echo "not ok 1 - vptree din99d fraction"
    exit 0
}

echo "ok 1 - vptree din99d fraction"
exit 0
