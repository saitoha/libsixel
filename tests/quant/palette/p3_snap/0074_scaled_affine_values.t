#!/bin/sh
# Verify P3 snap scaled affine values.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/p3_snap/0001_grid scaled 0 || {
    echo "not ok 1 - scaled affine values"
    exit 0
}
echo "ok 1 - scaled affine values"
exit 0
