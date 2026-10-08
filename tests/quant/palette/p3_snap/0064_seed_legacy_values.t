#!/bin/sh
# Verify P3 snap seed legacy values.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/p3_snap/0001_grid seed 0 || {
    echo "not ok 1 - seed legacy values"
    exit 0
}
echo "ok 1 - seed legacy values"
exit 0
