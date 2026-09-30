#!/bin/sh
# Verify native snap: extended din99d nearest partial.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/snap_float/0001_snap_contract unit 0 4 1 1 || {
    echo "not ok 1 - extended din99d nearest partial"
    exit 0
}

echo "ok 1 - extended din99d nearest partial"
exit 0
