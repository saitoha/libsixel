#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
# Compare stale lookup with an independent oracle for grid.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/0021_delaunay_byte_exact 64 2 sample stale || {
    printf '\n%s\n' "not ok 1 - delaunay byte stale grid indices"
    exit 0
}
printf '\n%s\n' "ok 1 - delaunay byte stale grid indices"
exit 0
