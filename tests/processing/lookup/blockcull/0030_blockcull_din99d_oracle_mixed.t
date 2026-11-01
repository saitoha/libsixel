#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
# Verify blockcull din99d oracle mixed.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/blockcull/0030_blockcull_din99d_oracle_mixed || {
    echo "not ok 1 - blockcull din99d oracle mixed"
    exit 0
}
echo "ok 1 - blockcull din99d oracle mixed"
exit 0
