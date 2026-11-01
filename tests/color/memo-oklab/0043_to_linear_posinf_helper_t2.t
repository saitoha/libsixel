#!/bin/sh
# oklab to linear posinf helper t2 matches the analytic conversion or preserves rejected input.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux

echo "1..1"
set -v

SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS=1
export SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    colorspace/float_exact 2 1 0 2 2 1 || {
    echo "not ok 1 - oklab to linear posinf helper t2 matches the analytic conversion or preserves rejected input"
    exit 0
}
echo "ok 1 - oklab to linear posinf helper t2 matches the analytic conversion or preserves rejected input"
exit 0
