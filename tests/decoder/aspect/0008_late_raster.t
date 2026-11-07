#!/bin/sh
# Verify late raster.
set -eux
echo "1..1"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/aspect/0008_late_raster" || {
    echo "not ok 1 - late raster"
    exit 0
}
echo "ok 1 - late raster"
exit 0
