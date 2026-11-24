#!/bin/sh
# Verify an empty visible sample set remains eligible for fallback.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0038_filter_binning_stream_empty" || {
    printf '\n%s\n' "not ok 1 - empty sample-stream binning fallback contract"
    exit 0
}

printf '\n%s\n' "ok 1 - empty sample-stream binning fallback contract"
exit 0
