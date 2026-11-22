#!/bin/sh
# Playback accepts an implicit sRGB palette for an explicit sRGB frame.
# Policy: docs/encoder/palette-construction/pipeline/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" planner/pipeline/0101_reserved_validation srgb implicit accept || {
    echo 'not ok 1 - Playback accepts an implicit sRGB palette for an explicit sRGB frame'
    exit 0
}
echo 'ok 1 - Playback accepts an implicit sRGB palette for an explicit sRGB frame'
exit 0
