#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify range palette boundary.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/adaptive_range/0020_range_palette_boundary" || {
    echo "not ok 1 - 0020_range_palette_boundary"
    exit 0
}
echo "ok 1 - 0020_range_palette_boundary"
exit 0
