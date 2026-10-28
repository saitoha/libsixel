#!/bin/sh
# Verify palette colors failure output.
# Policy: docs/encoder/palette-construction/pipeline/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "filter/0053_palette_colors_failure_output" || {
    echo "not ok 1 - palette colors failure output"
    exit 0
}
echo "ok 1 - palette colors failure output"
exit 0
