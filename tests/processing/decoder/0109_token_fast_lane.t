#!/bin/sh
# Policy: docs/decoder/pipeline/README.md
# Tokens consumed inside band runs decode as the general parser does.
set -eux
echo "1..1"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0109_token_fast_lane" || {
    echo "not ok 1 - band-run tokens match the general parser"
    exit 0
}
echo "ok 1 - band-run tokens match the general parser"
exit 0
