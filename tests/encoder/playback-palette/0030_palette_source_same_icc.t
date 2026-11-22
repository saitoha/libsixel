#!/bin/sh
# Playback accepts separate copies of the same RGB ICC profile.
# Policy: docs/encoder/palette-construction/pipeline/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" planner/pipeline/0101_reserved_validation icc icc accept || {
    echo 'not ok 1 - Playback accepts separate copies of the same RGB ICC profile'
    exit 0
}
echo 'ok 1 - Playback accepts separate copies of the same RGB ICC profile'
exit 0
