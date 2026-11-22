#!/bin/sh
# Gamma playback maps both Display P3 pixels to colors from the separate ICC palette source.
# Policy: docs/encoder/palette-construction/pipeline/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" planner/0003_default_colorspace_auto gamma-p3-playback || {
    echo 'not ok 1 - Gamma playback maps both Display P3 pixels to colors from the separate ICC palette source'
    exit 0
}
echo 'ok 1 - Gamma playback maps both Display P3 pixels to colors from the separate ICC palette source'
exit 0
