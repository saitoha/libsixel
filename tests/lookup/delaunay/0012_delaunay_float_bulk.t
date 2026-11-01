#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" lookup/0032_delaunay_float_bulk || {
    echo "not ok 1 - delaunay float_bulk"
    exit 0
}
echo "ok 1 - delaunay float_bulk"
exit 0
