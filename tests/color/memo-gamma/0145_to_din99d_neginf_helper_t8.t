#!/bin/sh
# gamma to din99d neginf helper t8 matches the analytic conversion or preserves rejected input.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux

echo "1..1"
set -v

SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS=1
export SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    colorspace/float_exact 2 2 0 0 0 4 || {
    printf '\n%s\n' "not ok 1 - gamma to din99d neginf helper t8 matches the analytic conversion or preserves rejected input"
    exit 0
}
printf '\n%s\n' "ok 1 - gamma to din99d neginf helper t8 matches the analytic conversion or preserves rejected input"
exit 0
