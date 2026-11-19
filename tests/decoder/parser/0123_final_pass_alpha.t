#!/bin/sh
# Policy: docs/decoder/pipeline/README.md
# The final pass's alpha answer agrees with the returned pixels, serial and
# threaded.
set -eux
echo "1..2"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0110_final_pass_alpha" || {
    echo "not ok 1 - serial final pass alpha matches the pixels"
    exit 0
}
echo "ok 1 - serial final pass alpha matches the pixels"
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=4 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0110_final_pass_alpha" || {
    echo "not ok 2 - threaded final pass alpha matches the pixels"
    exit 0
}
echo "ok 2 - threaded final pass alpha matches the pixels"
exit 0
