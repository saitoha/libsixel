#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify range highlight gate.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/adaptive_range/0021_range_highlight_gate" || {
    echo "not ok 1 - 0021_range_highlight_gate"
    exit 0
}
echo "ok 1 - 0021_range_highlight_gate"
exit 0
