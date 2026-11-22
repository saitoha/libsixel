#!/bin/sh
# Playback accepts an explicit sRGB palette for an implicit sRGB frame.
# Policy: docs/encoder/palette-construction/pipeline/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" planner/pipeline/0101_reserved_validation implicit srgb accept || {
    echo 'not ok 1 - Playback accepts an explicit sRGB palette for an implicit sRGB frame'
    exit 0
}
echo 'ok 1 - Playback accepts an explicit sRGB palette for an implicit sRGB frame'
exit 0
