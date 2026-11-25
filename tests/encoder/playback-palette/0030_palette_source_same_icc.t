#!/bin/sh
# Playback accepts separate copies of the same RGB ICC profile.
# Policy: docs/encoder/palette-construction/pipeline/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" planner/pipeline/0101_reserved_validation icc icc accept || {
    printf '\n%s\n' 'not ok 1 - Playback accepts separate copies of the same RGB ICC profile'
    exit 0
}
printf '\n%s\n' 'ok 1 - Playback accepts separate copies of the same RGB ICC profile'
exit 0
