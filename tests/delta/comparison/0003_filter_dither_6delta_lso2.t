#!/bin/sh
# Run the LSO2 6delta regression via the unified runner.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0015_filter_dither_6delta_lso2" || {
    printf '\n%s\n' "not ok 1 - 0015_filter_dither_6delta_lso2"
    exit 0
}

printf '\n%s\n' "ok 1 - 0015_filter_dither_6delta_lso2"
exit 0
