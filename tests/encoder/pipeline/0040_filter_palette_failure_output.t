#!/bin/sh
# Verify that a failed palette builder cannot leak a partial dither output.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0030_filter_palette_failure_output" || {
    printf '\n%s\n' "not ok 1 - 0030_filter_palette_failure_output"
    exit 0
}

printf '\n%s\n' "ok 1 - 0030_filter_palette_failure_output"
exit 0
