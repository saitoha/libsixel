#!/bin/sh
# Run the focused alpha-zero sample-stream binning test.
# Policy: docs/concepts/pixelformat.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0042_filter_binning_stream_alpha_zero" || {
    printf '\n%s\n' "not ok 1 - sample-stream alpha-zero exclusion"
    exit 0
}

printf '\n%s\n' "ok 1 - sample-stream alpha-zero exclusion"
exit 0
