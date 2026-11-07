#!/bin/sh
# Verify first raster wins.
set -eux
echo "1..1"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/aspect/0007_first_raster_wins" || {
    echo "not ok 1 - first raster wins"
    exit 0
}
echo "ok 1 - first raster wins"
exit 0
