#!/bin/sh
# Policy: docs/decoder/pipeline/README.md
# Verify normal columns growth clip.
set -eux
echo "1..1"
set -v

SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0097_normal_columns_growth_clip" || {
    printf '\n%s\n' "not ok 1 - normal_columns_growth_clip"
    exit 0
}
printf '\n%s\n' "ok 1 - normal_columns_growth_clip"
exit 0
