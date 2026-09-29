#!/bin/sh
# Verify certlut din99d range.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/extended/0001_extended_lookup range 0 4 || {
    echo "not ok 1 - certlut din99d range"
    exit 0
}

echo "ok 1 - certlut din99d range"
exit 0
