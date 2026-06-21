#!/bin/sh
# Run the OR-mode body full-band unit test via the unified runner.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0002_encoder_core_ormode_body_full_band" || {
    echo "not ok 1 - 0002_encoder_core_ormode_body_full_band"
    exit 0
}

echo "ok 1 - 0002_encoder_core_ormode_body_full_band"
exit 0
