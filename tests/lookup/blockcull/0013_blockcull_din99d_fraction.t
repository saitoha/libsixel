#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
# Verify blockcull din99d fraction.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/blockcull/0010_blockcull_din99d_fraction || {
    echo "not ok 1 - blockcull din99d fraction"
    exit 0
}
echo "ok 1 - blockcull din99d fraction"
exit 0
