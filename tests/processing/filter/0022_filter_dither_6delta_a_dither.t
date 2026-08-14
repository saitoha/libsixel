#!/bin/sh
# Run the A-dither 6delta regression via the unified runner.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0022_filter_dither_6delta_a_dither" || {
    echo "not ok 1 - 0022_filter_dither_6delta_a_dither"
    exit 0
}

echo "ok 1 - 0022_filter_dither_6delta_a_dither"
exit 0
