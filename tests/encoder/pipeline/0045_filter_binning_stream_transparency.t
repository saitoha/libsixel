#!/bin/sh
# Verify sample-stream binning excludes explicitly masked samples.
# Policy: docs/concepts/pixelformat.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0036_filter_binning_stream_transparency" || {
    printf '\n%s\n' "not ok 1 - sample-stream transparent-mask contract"
    exit 0
}

printf '\n%s\n' "ok 1 - sample-stream transparent-mask contract"
exit 0
