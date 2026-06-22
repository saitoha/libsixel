#!/bin/sh
# Run the OR-mode empty-plane skip unit test via the unified runner.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0006_encoder_core_ormode_body_skips_empty_planes" || {
    echo "not ok 1 - 0006_encoder_core_ormode_body_skips_empty_planes"
    exit 0
}

echo "ok 1 - 0006_encoder_core_ormode_body_skips_empty_planes"
exit 0
