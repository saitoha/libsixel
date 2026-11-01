#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Verify ordered din99d range.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/ordered/0005_ord_din99d_range || {
    echo "not ok 1 - ordered din99d range"
    exit 0
}
echo "ok 1 - ordered din99d range"
exit 0
