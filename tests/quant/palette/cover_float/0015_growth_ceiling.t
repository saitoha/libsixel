#!/bin/sh
# Verify native soft cover: growth ceiling.
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/cover_float/0001_cover_contract values 6 0 0 || {
    echo "not ok 1 - growth ceiling"
    exit 0
}

echo "ok 1 - growth ceiling"
exit 0
