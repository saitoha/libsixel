#!/bin/sh
# Run the OR-mode body tail-band unit test via the unified runner.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0003_encoder_core_ormode_body_tail_band" || {
    echo "not ok 1 - 0003_encoder_core_ormode_body_tail_band"
    exit 0
}

echo "ok 1 - 0003_encoder_core_ormode_body_tail_band"
exit 0
