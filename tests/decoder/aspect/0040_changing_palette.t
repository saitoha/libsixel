#!/bin/sh
# Verify changing palette.
set -eux
echo "1..1"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/aspect/0040_changing_palette" || {
    printf '\n%s\n' "not ok 1 - changing palette"
    exit 0
}
printf '\n%s\n' "ok 1 - changing palette"
exit 0
