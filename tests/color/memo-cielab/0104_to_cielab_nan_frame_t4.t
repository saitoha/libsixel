#!/bin/sh
# cielab to cielab nan frame t4 matches the analytic conversion or preserves rejected input.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux

echo "1..1"
set -v

SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS=1
export SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    colorspace/float_exact 2 0 1 3 3 3 || {
    printf '\n%s\n' "not ok 1 - cielab to cielab nan frame t4 matches the analytic conversion or preserves rejected input"
    exit 0
}
printf '\n%s\n' "ok 1 - cielab to cielab nan frame t4 matches the analytic conversion or preserves rejected input"
exit 0
