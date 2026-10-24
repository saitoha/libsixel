#!/bin/sh
# Policy: docs/decoder/pipeline/README.md
# Verify normal columns band ranges.
set -eux
echo "1..1"
set -v

SIXEL_THREADING_GLOBAL_BUDGET=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0098_normal_columns_band_ranges" || {
    echo "not ok 1 - normal_columns_band_ranges"
    exit 0
}
echo "ok 1 - normal_columns_band_ranges"
exit 0
