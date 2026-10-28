#!/bin/sh
# Verify palette colors hidden nonfinite.
# Policy: docs/encoder/palette-construction/pipeline/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "filter/0052_palette_colors_hidden_nonfinite" || {
    echo "not ok 1 - palette colors hidden nonfinite"
    exit 0
}
echo "ok 1 - palette colors hidden nonfinite"
exit 0
