#!/bin/sh
# Verify palette colors statistics disabled.
# Policy: docs/encoder/palette-construction/pipeline/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "filter/0051_palette_colors_statistics_disabled" || {
    echo "not ok 1 - palette colors statistics disabled"
    exit 0
}
echo "ok 1 - palette colors statistics disabled"
exit 0
