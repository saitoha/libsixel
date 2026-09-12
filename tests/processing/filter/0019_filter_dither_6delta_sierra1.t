#!/bin/sh
# Run the Sierra Lite 6delta regression via the unified runner.
# Policy: docs/functionality/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0019_filter_dither_6delta_sierra1" || {
    echo "not ok 1 - 0019_filter_dither_6delta_sierra1"
    exit 0
}

echo "ok 1 - 0019_filter_dither_6delta_sierra1"
exit 0
