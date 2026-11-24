#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify adaptive-grid quality clip RGB definitions across thread budgets.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/sampling_threads/0001_palette_invariance" adaptive-grid 1 1 || {
    printf '\n%s\n' "not ok 1 - adaptive-grid quality clip palette thread invariance"
    exit 0
}
printf '\n%s\n' "ok 1 - adaptive-grid quality clip palette thread invariance"
exit 0
