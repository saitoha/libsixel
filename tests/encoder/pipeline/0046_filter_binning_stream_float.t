#!/bin/sh
# Verify float sample-stream coordinates and mapping parameters are validated.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0037_filter_binning_stream_float" || {
    printf '\n%s\n' "not ok 1 - sample-stream float contract"
    exit 0
}

printf '\n%s\n' "ok 1 - sample-stream float contract"
exit 0
