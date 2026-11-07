#!/bin/sh
# Verify changing untrusted growth.
set -eux
echo "1..1"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/aspect/0037_changing_untrusted_growth" || {
    echo "not ok 1 - changing untrusted growth"
    exit 0
}
echo "ok 1 - changing untrusted growth"
exit 0
