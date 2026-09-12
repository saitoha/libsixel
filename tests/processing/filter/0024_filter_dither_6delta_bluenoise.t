#!/bin/sh
# Run the blue-noise 6delta regression via the unified runner.
# Policy: docs/functionality/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0024_filter_dither_6delta_bluenoise" || {
    echo "not ok 1 - 0024_filter_dither_6delta_bluenoise"
    exit 0
}

echo "ok 1 - 0024_filter_dither_6delta_bluenoise"
exit 0
