#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
# Compare scalar lookup with an independent oracle for random_40.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/0021_delaunay_byte_exact 40 0 sample scalar || {
    echo "not ok 1 - delaunay byte scalar random_40 indices"
    exit 0
}
echo "ok 1 - delaunay byte scalar random_40 indices"
exit 0
