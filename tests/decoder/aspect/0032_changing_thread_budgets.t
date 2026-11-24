#!/bin/sh
# Verify changing thread budgets.
set -eux
echo "1..1"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/aspect/0032_changing_thread_budgets" || {
    printf '\n%s\n' "not ok 1 - changing thread budgets"
    exit 0
}
printf '\n%s\n' "ok 1 - changing thread budgets"
exit 0
