#!/bin/sh
# Policy: docs/decoder/pipeline/README.md
# The final pass's alpha answer agrees with the decoded pixels.
set -eux
echo '1..1'
set -v

SIXEL_DECODER_THREADING_POLICY=greedy \
    SIXEL_DECODER_THREADING_GLOBAL_BUDGET=4 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0110_final_pass_alpha" || {
    echo 'not ok 1 - threaded final pass alpha matches the pixels'
    exit 0
}
echo 'ok 1 - threaded final pass alpha matches the pixels'
exit 0
