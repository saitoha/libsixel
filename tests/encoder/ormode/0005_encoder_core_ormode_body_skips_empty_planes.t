#!/bin/sh
# Policy: docs/encoder/output/or-mode/README.md
# Run the OR-mode empty-plane skip unit test via the unified runner.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0006_encoder_core_ormode_body_skips_empty_planes" || {
    printf '\n%s\n' "not ok 1 - 0006_encoder_core_ormode_body_skips_empty_planes"
    exit 0
}

printf '\n%s\n' "ok 1 - 0006_encoder_core_ormode_body_skips_empty_planes"
exit 0
