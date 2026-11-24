#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify auto quality clip RGB definitions across thread budgets.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/sampling_threads/0001_palette_invariance" auto 1 1 || {
    printf '\n%s\n' "not ok 1 - auto quality clip palette thread invariance"
    exit 0
}
printf '\n%s\n' "ok 1 - auto quality clip palette thread invariance"
exit 0
