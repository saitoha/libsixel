#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
# Compare map lookup with an independent oracle for grid.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/0021_delaunay_byte_exact 64 2 sample map || {
    echo "not ok 1 - delaunay byte map grid indices"
    exit 0
}
echo "ok 1 - delaunay byte map grid indices"
exit 0
