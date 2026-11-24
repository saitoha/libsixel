#!/bin/sh
# Verify exact binning aggregates sample-stream coordinates without rounding.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0040_filter_binning_stream_exact" || {
    printf '\n%s\n' "not ok 1 - exact sample-stream binning contract"
    exit 0
}

printf '\n%s\n' "ok 1 - exact sample-stream binning contract"
exit 0
