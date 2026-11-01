#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
# Verify blockcull din99d range.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/blockcull/0005_blockcull_din99d_range || {
    echo "not ok 1 - blockcull din99d range"
    exit 0
}
echo "ok 1 - blockcull din99d range"
exit 0
