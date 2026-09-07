#!/bin/sh
# Run the focused alpha-zero sample-stream binning test.
# Policy: docs/concepts/pixelformat.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0042_filter_binning_stream_alpha_zero" || {
    echo "not ok 1 - sample-stream alpha-zero exclusion"
    exit 0
}

echo "ok 1 - sample-stream alpha-zero exclusion"
exit 0
