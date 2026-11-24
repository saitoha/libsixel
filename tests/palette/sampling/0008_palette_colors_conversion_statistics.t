#!/bin/sh
# Verify palette colors conversion statistics.
# Policy: docs/encoder/palette-construction/pipeline/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "filter/0050_palette_colors_conversion_statistics" || {
    printf '\n%s\n' "not ok 1 - palette colors conversion statistics"
    exit 0
}
printf '\n%s\n' "ok 1 - palette colors conversion statistics"
exit 0
