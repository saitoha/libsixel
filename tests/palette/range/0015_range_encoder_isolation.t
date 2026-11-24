#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify range encoder isolation.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/adaptive_range/0015_range_encoder_isolation" || {
    printf '\n%s\n' "not ok 1 - 0015_range_encoder_isolation"
    exit 0
}
printf '\n%s\n' "ok 1 - 0015_range_encoder_isolation"
exit 0
