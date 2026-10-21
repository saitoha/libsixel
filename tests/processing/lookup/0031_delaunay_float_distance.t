#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" lookup/0031_delaunay_float_distance || {
    echo "not ok 1 - delaunay float_distance"
    exit 0
}
echo "ok 1 - delaunay float_distance"
exit 0
