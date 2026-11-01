#!/bin/sh
# Verify float memo extended exact against uncached analytic conversion.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux

echo "1..1"
set -v
SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS=1
export SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "colorspace/float_exact" "1" || {
    echo "not ok 1 - float memo extended exact"
    exit 0
}
echo "ok 1 - float memo extended exact"
exit 0
