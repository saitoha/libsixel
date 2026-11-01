#!/bin/sh
# Verify that a failed palette builder cannot leak a partial dither output.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0030_filter_palette_failure_output" || {
    echo "not ok 1 - 0030_filter_palette_failure_output"
    exit 0
}

echo "ok 1 - 0030_filter_palette_failure_output"
exit 0
