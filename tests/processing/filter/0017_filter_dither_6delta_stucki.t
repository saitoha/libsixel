#!/bin/sh
# Run the Stucki 6delta regression via the unified runner.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0017_filter_dither_6delta_stucki" || {
    echo "not ok 1 - 0017_filter_dither_6delta_stucki"
    exit 0
}

echo "ok 1 - 0017_filter_dither_6delta_stucki"
exit 0
