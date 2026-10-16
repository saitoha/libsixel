#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify adaptive-grid default down4 RGB definitions across thread budgets.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/sampling_threads/0001_palette_invariance" adaptive-grid 0 2 || {
    echo "not ok 1 - adaptive-grid default down4 palette thread invariance"
    exit 0
}
echo "ok 1 - adaptive-grid default down4 palette thread invariance"
exit 0
