#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
# Verify blockcull din99d metric.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/blockcull/0032_blockcull_din99d_metric || {
    echo "not ok 1 - blockcull din99d metric"
    exit 0
}
echo "ok 1 - blockcull din99d metric"
exit 0
