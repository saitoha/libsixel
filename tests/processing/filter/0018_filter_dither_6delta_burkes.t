#!/bin/sh
# Run the Burkes 6delta regression via the unified runner.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0018_filter_dither_6delta_burkes" || {
    echo "not ok 1 - 0018_filter_dither_6delta_burkes"
    exit 0
}

echo "ok 1 - 0018_filter_dither_6delta_burkes"
exit 0
