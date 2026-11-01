#!/bin/sh
# Verify native cover: hard oklab midpoint.
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/cover_float/0001_cover_contract hard 4 2 0 || {
    echo "not ok 1 - hard oklab midpoint"
    exit 0
}

echo "ok 1 - hard oklab midpoint"
exit 0
