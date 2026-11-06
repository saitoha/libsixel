#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu/delaunay_fractional_cache || {
    echo "not ok 1 - delaunay fractional cache"
    exit 0
}
echo "ok 1 - delaunay fractional cache"
exit 0
