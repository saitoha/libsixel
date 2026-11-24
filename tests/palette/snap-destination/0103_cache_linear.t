#!/bin/sh
# Verify P3 destination grid: cache linear.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/p3_snap/0001_grid 3 1 || {
    printf '\n%s\n' "not ok 1 - P3 grid cache linear"
    exit 0
}

printf '\n%s\n' "ok 1 - P3 grid cache linear"
exit 0
