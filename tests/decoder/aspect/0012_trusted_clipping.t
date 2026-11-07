#!/bin/sh
# Verify trusted clipping.
set -eux
echo "1..1"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/aspect/0012_trusted_clipping" || {
    echo "not ok 1 - trusted clipping"
    exit 0
}
echo "ok 1 - trusted clipping"
exit 0
