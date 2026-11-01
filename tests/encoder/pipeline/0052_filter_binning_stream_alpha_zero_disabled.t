#!/bin/sh
# Run the focused disabled alpha-zero interpretation control.
# Policy: docs/concepts/pixelformat.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0045_filter_binning_stream_alpha_zero_disabled" || {
    echo "not ok 1 - disabled alpha-zero interpretation control"
    exit 0
}

echo "ok 1 - disabled alpha-zero interpretation control"
exit 0
