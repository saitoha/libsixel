#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify range recoverable tones.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/adaptive_range/0018_range_recoverable_tones" || {
    echo "not ok 1 - 0018_range_recoverable_tones"
    exit 0
}
echo "ok 1 - 0018_range_recoverable_tones"
exit 0
